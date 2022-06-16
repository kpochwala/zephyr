/* vl53l0x.c - Driver for ST VL53L0X time of flight sensor */

#define DT_DRV_COMPAT st_vl53l0x

/*
 * Copyright (c) 2017 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"

LOG_MODULE_REGISTER(VL53L0X, CONFIG_SENSOR_LOG_LEVEL);

/* All the values used in this driver are coming from ST datasheet and examples.
 * It can be found here:
 *   http://www.st.com/en/embedded-software/stsw-img005.html
 * There are also examples of use in the L4 cube FW:
 *   http://www.st.com/en/embedded-software/stm32cubel4.html
 */
#define VL53L0X_INITIAL_ADDR                    0x29
#define VL53L0X_REG_WHO_AM_I                    0xC0
#define VL53L0X_CHIP_ID                         0xEEAA
#define VL53L0X_SETUP_SIGNAL_LIMIT              (0.1 * 65536)
#define VL53L0X_SETUP_SIGMA_LIMIT               (60 * 65536)
#define VL53L0X_SETUP_MAX_TIME_FOR_RANGING      33000
#define VL53L0X_SETUP_PRE_RANGE_VCSEL_PERIOD    18
#define VL53L0X_SETUP_FINAL_RANGE_VCSEL_PERIOD  14

struct vl53l0x_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec xshut;
	struct gpio_dt_spec gpio1;
};

struct vl53l0x_data {
	bool started;
	VL53L0X_Dev_t vl53l0x;
	VL53L0X_RangingMeasurementData_t RangingMeasurementData;
	VL53L0X_DeviceModes DeviceMode;
};

static int vl53l0x_perform_offset_calibration(const struct device *dev, const struct sensor_value *distance)
{
	int ret;
	struct vl53l0x_data *drv_data = dev->data;

	return ret;
}

static int vl53l0x_perform_xtalk_calibration(const struct device *dev, const struct sensor_value *distance)
{
	int ret;
	struct vl53l0x_data *drv_data = dev->data;

	return ret;
}

static int vl53l0x_setup_continous(const struct device* dev)
{
	int ret;
	struct vl53l0x_data *drv_data = dev->data;

	ret = VL53L0X_SetDeviceMode(&drv_data->vl53l0x,
				    VL53L0X_DEVICEMODE_CONTINUOUS_RANGING);
	if (ret) {
		LOG_ERR("[%s] VL53L0X_SetDeviceMode CONTINOUS failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

static int vl53l0x_setup_single(const struct device* dev)
{
	int ret;
	struct vl53l0x_data *drv_data = dev->data;

	ret = VL53L0X_SetDeviceMode(&drv_data->vl53l0x,
				    VL53L0X_DEVICEMODE_SINGLE_RANGING);
	if (ret) {
		LOG_ERR("[%s] VL53L0X_SetDeviceMode CONTINOUS failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

static int vl53l0x_setup_continous_timed(const struct device* dev, const struct sensor_value *delay)
{
	int ret;
	struct vl53l0x_data *drv_data = dev->data;

	return ret;
}

// example range profiles, check UM2039 p. 22 "Example API range profiles"
enum range_profile
{
	DEFAULT_MODE = 0, // 30ms  time budget, max range 1200 mm
	HIGH_ACCURACY,    // 200ms time budget, max range 1200 mm
	LONG_RANGE,       // 33ms  time budget, max range 2000 mm
	HIGH_SPEED        // 20ms  time budget, max range 1200 mm
};

static int vl53l0x_set_profile_values(const struct device* dev, float signal_rate_final_mcps, uint32_t sigma_final_range_mm, uint32_t timing_budget_ms)
{
	int ret;
	struct vl53l0x_data *drv_data = dev->data;
	
	// api values are scaled, check UM2039 p. 22 "Example API range profiles"
	uint32_t signal_rate_final_api = signal_rate_final_mcps * 65536;
	uint32_t sigma_final_range_api = sigma_final_range_mm * 65536;
	uint32_t timing_budget_api = timing_budget_ms * 1000;


	// set limit values
	ret = VL53L0X_SetLimitCheckValue(&drv_data->vl53l0x, VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, signal_rate_final_api);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0X_SetLimitCheckValue SIGNAL_RATE_FINAL_RANGE failed",
			dev->name);
		goto exit;
	}

	ret = VL53L0X_SetLimitCheckValue(&drv_data->vl53l0x, VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, sigma_final_range_api);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0X_SetLimitCheckValue SIGMA_FINAL_RANGE failed",
			dev->name);
		goto exit;
	}

	ret = VL53L0X_SetMeasurementTimingBudgetMicroSeconds(&drv_data->vl53l0x, timing_budget_api);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0_SetMeasurementTimingBudgetMicroSeconds failed",
			dev->name);
		goto exit;
	}

	// enable limits
	ret = VL53L0X_SetLimitCheckEnable(&drv_data->vl53l0x, VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0X_SetLimitCheckEnable SIGNAL_RATE_FINAL_RANGE failed",
			dev->name);
		goto exit;
	}
	
	ret = VL53L0X_SetLimitCheckEnable(&drv_data->vl53l0x, VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0X_SetLimitCheckEnable SIGMA_FINAL_RANGE failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

// todo: set real default values
static int vl53l0x_setup_range_profile_default(const struct device* dev)
{
	int ret;
	
	ret = vl53l0x_set_profile_values(dev, 0.25, 18, 30);
	if(ret)
	{
		LOG_ERR("[%s] vl53l0x_set_profile_values failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

// signal rate 0.25Mcps, sigma 18mm, timing budget 30ms
static int vl53l0x_setup_range_profile_high_accuracy(const struct device* dev)
{
	int ret;
	
	ret = vl53l0x_set_profile_values(dev, 0.25, 18, 30);
	if(ret)
	{
		LOG_ERR("[%s] vl53l0x_set_profile_values failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

// signal rate 0.1Mcps, sigma 60mm, timing budget 33ms
static int vl53l0x_setup_range_profile_long_range(const struct device* dev)
{
	int ret;
	struct vl53l0x_data *drv_data = dev->data;

	ret = vl53l0x_set_profile_values(dev, 0.25, 18, 30);
	if(ret)
	{
		LOG_ERR("[%s] vl53l0x_set_profile_values failed",
			dev->name);
		goto exit;
	}

	// set VCSEL values
	ret = VL53L0X_SetVcselPulsePeriod(&drv_data->vl53l0x, VL53L0X_VCSEL_PERIOD_PRE_RANGE, 18);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0X_SetVcselPulsePeriod VL53L0X_VCSEL_PERIOD_PRE_RANGE failed",
			dev->name);
		goto exit;
	}

	ret = VL53L0X_SetVcselPulsePeriod(&drv_data->vl53l0x, VL53L0X_VCSEL_PERIOD_FINAL_RANGE, 14);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0X_SetVcselPulsePeriod VL53L0X_VCSEL_PERIOD_FINAL_RANGE failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

// signal rate 0.25Mcps, sigma 18mm, timing budget 30ms
static int vl53l0x_setup_range_profile_high_speed(const struct device* dev)
{
	int ret;
	
	ret = vl53l0x_set_profile_values(dev, 0.25, 18, 30);
	if(ret)
	{
		LOG_ERR("[%s] vl53l0x_set_profile_values failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

// set up range profile based on enum range_profile
static int vl53l0x_setup_range_profile(const struct device* dev, const enum range_profile profile)
{
	int ret = 0;

	if(dev == NULL)
	{
		// set ret to nosup?
		LOG_ERR("[] vl53l0x_setup_range_profile, null device passed", NULL);
		goto exit;
	}

	switch(profile)
	{
		case DEFAULT_MODE: 
			ret = vl53l0x_setup_range_profile_default(dev);
			if(ret)
			{
				LOG_ERR("[%s] vl53l0x_setup_range_profile_default failed",
					dev->name);
				goto exit;
			}
			break;
		case HIGH_ACCURACY: 
			ret = vl53l0x_setup_range_profile_high_accuracy(dev);
			if(ret)
			{
				LOG_ERR("[%s] vl53l0x_setup_range_profile_high_accuracy failed",
					dev->name);
				goto exit;
			}
			break;
		case LONG_RANGE: 
			ret = vl53l0x_setup_range_profile_long_range(dev);
			if(ret)
			{
				LOG_ERR("[%s] vl53l0x_setup_range_profile_long_range failed",
					dev->name);
				goto exit;
			}
			break;
		case HIGH_SPEED: 
			ret = vl53l0x_setup_range_profile_high_speed(dev);
			if(ret)
			{
				LOG_ERR("[%s] vl53l0x_setup_range_profile_high_speed failed",
					dev->name);
				goto exit;
			}
			break;
		default:
			ret = -ENOTSUP;
			LOG_ERR("[%s] vl53l0x_setup_range_profile failed, unknown profile: %d",
				dev->name, profile);
			goto exit;
			break;
	}
exit:
	return ret;
}

// perform basic calibration (without crosstalk and offset)
static int vl53l0x_perform_basic_calibration(const struct device* dev)
{
	struct vl53l0x_data *drv_data = dev->data;
	int ret;

	uint8_t VhvSettings;
	uint8_t PhaseCal;
	uint32_t refSpadCount;
	uint8_t isApertureSpads;

	ret = VL53L0X_PerformRefCalibration(&drv_data->vl53l0x,
					&VhvSettings,
					&PhaseCal);
	if (ret) {
		LOG_ERR("[%s] VL53L0X_PerformRefCalibration failed",
			dev->name);
		goto exit;
	}

	ret = VL53L0X_PerformRefSpadManagement(&drv_data->vl53l0x,
					       (uint32_t *)&refSpadCount,
					       &isApertureSpads);
	if (ret) {
		LOG_ERR("[%s] VL53L0X_PerformRefSpadManagement failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;	
}

// device initialization UM2039 3.2.Fig 5
static int vl53l0x_device_initialization(const struct device* dev)
{
	struct vl53l0x_data *drv_data = dev->data;
	int ret;

	ret = VL53L0X_DataInit(&drv_data->vl53l0x);
	if (ret) {
		LOG_ERR("[%s] VL53L0X_DataInit failed, return error (%d)",
			dev->name, ret);
		goto exit;
	}
	
	ret = VL53L0X_StaticInit(&drv_data->vl53l0x);
	if (ret) {
		LOG_ERR("[%s] VL53L0X_StaticInit failed, return error: (%d)",
			dev->name, ret);
		goto exit;
	}

exit:
	return ret;
}

// set gpio to be used as input
static int vl53l0x_setup_gpio(const struct device* dev)
{
	int ret = 0;
	const struct vl53l0x_config *const config = dev->config;
	if(config->gpio1.port == NULL){
		LOG_ERR("[%s] No or improper gpio1 specified in config",
			dev->name);
		ret = -EINVAL;
		goto exit;
	}

	gpio_flags_t flags = GPIO_INPUT | GPIO_PULL_UP;

	ret = gpio_pin_configure(config->gpio1.port, config->gpio1.pin, flags);
	if(ret){
		LOG_ERR("[%s] gpio_pin_configure failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}


static int vl53l0x_setup(const struct device *dev)
{
	struct vl53l0x_data *drv_data = dev->data;
	int ret;

	ret = vl53l0x_device_initialization(dev);
	if (ret) {
		LOG_ERR("[%s] vl53l0x_device_initialization failed",
			dev->name);
		goto exit;
	}

	ret = vl53l0x_perform_basic_calibration(dev);
	if (ret) {
		LOG_ERR("[%s] vl53l0x_device_initialization failed",
			dev->name);
		goto exit;
	}

	#ifdef CONFIG_VL53L0X_GPIO_IN_RANGE
	ret = VL53L0X_SetGpioConfig(&drv_data->vl53l0x,
								0,
								drv_data->DeviceMode,
								VL53L0X_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW,
								VL53L0X_INTERRUPTPOLARITY_LOW
								);
		if (ret) {
		LOG_ERR("[%s] vl53l0x_setup_continous failed",
			dev->name);
		goto exit;
	}

	ret = VL53L0X_SetInterruptThresholds(&drv_data->vl53l0x,
								drv_data->DeviceMode,
								CONFIG_VL53L0X_PROXIMITY_THRESHOLD,
								10000
								);
		if (ret) {
		LOG_ERR("[%s] vl53l0x_setup_continous failed",
			dev->name);
		goto exit;
	}
	#endif


	#ifdef CONFIG_VL53L0X_CONTINOUS_RANGE

	ret = vl53l0x_setup_continous(dev);
		if (ret) {
		LOG_ERR("[%s] vl53l0x_setup_continous failed",
			dev->name);
		goto exit;
	}

	#else
	
	ret = vl53l0x_setup_single(dev);
	if (ret) {
		LOG_ERR("[%s] vl53l0x_setup_single failed",
			dev->name);
		goto exit;
	}
	
	#endif

	ret = VL53L0X_GetDeviceMode(&drv_data->vl53l0x, &drv_data->DeviceMode);
	if (ret) {
		LOG_ERR("[%s] VL53L0X_GetDeviceMode failed",
			dev->name);
		goto exit;
	}

	ret = vl53l0x_setup_range_profile(dev, HIGH_SPEED);
	if (ret) {
		LOG_ERR("[%s] vl53l0x_setup_range_profile failed",
			dev->name);
		goto exit;
	}

	ret = VL53L0X_StartMeasurement(&drv_data->vl53l0x);
	if (ret) {
		LOG_ERR("[%s] vl53l0x_setup_range_profile failed",
			dev->name);
		goto exit;
	}

exit:
	return ret;
}

static int vl53l0x_start(const struct device *dev)
{
	const struct vl53l0x_config *const config = dev->config;
	struct vl53l0x_data *drv_data = dev->data;
	int r;
	VL53L0X_Error ret;
	uint16_t vl53l0x_id = 0U;
	VL53L0X_DeviceInfo_t vl53l0x_dev_info;

	LOG_DBG("[%s] Starting", dev->name);

	/* Pull XSHUT high to start the sensor */
	if (config->xshut.port) {
		r = gpio_pin_set_dt(&config->xshut, 1);
		if (r < 0) {
			LOG_ERR("[%s] Unable to set XSHUT gpio (error %d)",
				dev->name, r);
			return -EIO;
		}
		k_sleep(K_MSEC(2));
	}

#ifdef CONFIG_VL53L0X_RECONFIGURE_ADDRESS
	if (config->i2c.addr != VL53L0X_INITIAL_ADDR) {
		ret = VL53L0X_SetDeviceAddress(&drv_data->vl53l0x, 2 * config->i2c.addr);
		if (ret != 0) {
			LOG_ERR("[%s] Unable to reconfigure I2C address",
				dev->name);
			return -EIO;
		}

		drv_data->vl53l0x.I2cDevAddr = config->i2c.addr;
		LOG_DBG("[%s] I2C address reconfigured", dev->name);
		k_sleep(K_MSEC(2));
	}
#endif

	/* Get info from sensor */
	(void)memset(&vl53l0x_dev_info, 0, sizeof(VL53L0X_DeviceInfo_t));

	ret = VL53L0X_GetDeviceInfo(&drv_data->vl53l0x, &vl53l0x_dev_info);
	if (ret < 0) {
		LOG_ERR("[%s] Could not get info from device.", dev->name);
		return -ENODEV;
	}

	LOG_DBG("[%s] VL53L0X_GetDeviceInfo = %d", dev->name, ret);
	LOG_DBG("   Device Name : %s", vl53l0x_dev_info.Name);
	LOG_DBG("   Device Type : %s", vl53l0x_dev_info.Type);
	LOG_DBG("   Device ID : %s", vl53l0x_dev_info.ProductId);
	LOG_DBG("   ProductRevisionMajor : %d",
		vl53l0x_dev_info.ProductRevisionMajor);
	LOG_DBG("   ProductRevisionMinor : %d",
		vl53l0x_dev_info.ProductRevisionMinor);

	ret = VL53L0X_RdWord(&drv_data->vl53l0x,
			     VL53L0X_REG_WHO_AM_I,
			     (uint16_t *) &vl53l0x_id);
	if ((ret < 0) || (vl53l0x_id != VL53L0X_CHIP_ID)) {
		LOG_ERR("[%s] Issue on device identification", dev->name);
		return -ENOTSUP;
	}

	ret = vl53l0x_setup(dev);
	if (ret < 0) {
		return -ENOTSUP;
	}


#ifdef CONFIG_VL53L0X_GPIO_IN_RANGE
	ret = vl53l0x_setup_gpio(dev);
#endif

	drv_data->started = true;
	LOG_DBG("[%s] Started", dev->name);
	return 0;
}

static int vl53l0x_sample_fetch(const struct device *dev,
				enum sensor_channel chan)
{
	struct vl53l0x_data *drv_data = dev->data;
	VL53L0X_Error ret;
	int r;

	__ASSERT_NO_MSG((chan == SENSOR_CHAN_ALL)
			|| (chan == SENSOR_CHAN_DISTANCE)
			|| (chan == SENSOR_CHAN_PROX));

	if (!drv_data->started) {
		r = vl53l0x_start(dev);
		if (r < 0) {
			return r;
		}
	}

	bool should_read = true;
	#ifdef CONFIG_VL53L0X_GPIO_IN_RANGE
	const struct vl53l0x_config *const config = dev->config;
	int gpio_value = gpio_pin_get(config->gpio1.port, config->gpio1.pin);
	if(gpio_value < 0)
	{
		LOG_ERR("[%s] gpio_pin_get failed",
			dev->name);
		return gpio_value;
	}
	should_read = gpio_value;
	#endif

	if(should_read){
	#ifdef CONFIG_VL53L0X_CONTINOUS_RANGE
		ret = VL53L0X_GetRangingMeasurementData(&drv_data->vl53l0x,
								&drv_data->RangingMeasurementData);
	#else
		ret = VL53L0X_PerformSingleRangingMeasurement(&drv_data->vl53l0x,
								&drv_data->RangingMeasurementData);
	#endif
	#ifdef CONFIG_VL53L0X_GPIO_IN_RANGE
	ret = VL53L0X_ClearInterruptMask(&drv_data->vl53l0x, 0);
	if(ret)
	{
		LOG_ERR("[%s] VL53L0X_ClearInterruptMask failed",
			dev->name);
		return gpio_value;
	}
	#endif
	}else{
		drv_data->RangingMeasurementData.RangeMilliMeter = 9999;
		drv_data->RangingMeasurementData.RangeFractionalPart = 0;
	}

	if (ret < 0) {
		LOG_ERR("[%s] Could not perform measurment (error=%d)",
			dev->name, ret);
		return -EINVAL;
	}



	return 0;
}


static int vl53l0x_channel_get(const struct device *dev,
			       enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct vl53l0x_data *drv_data = dev->data;

	if (chan == SENSOR_CHAN_PROX) {
		if (drv_data->RangingMeasurementData.RangeMilliMeter <=
		    CONFIG_VL53L0X_PROXIMITY_THRESHOLD) {
			val->val1 = 1;
		} else {
			val->val1 = 0;
		}
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_DISTANCE) {
		val->val1 = drv_data->RangingMeasurementData.RangeMilliMeter / 1000;
		val->val2 = (drv_data->RangingMeasurementData.RangeMilliMeter % 1000) * 1000;
	} else {
		return -ENOTSUP;
	}

	return 0;
}

static const struct sensor_driver_api vl53l0x_api_funcs = {
	.sample_fetch = vl53l0x_sample_fetch,
	.channel_get = vl53l0x_channel_get,
};

static int vl53l0x_init(const struct device *dev)
{
	int r;
	struct vl53l0x_data *drv_data = dev->data;
	const struct vl53l0x_config *const config = dev->config;

	/* Initialize the HAL peripheral with the default sensor address,
	 * ie. the address on power up
	 */
	drv_data->vl53l0x.I2cDevAddr = VL53L0X_INITIAL_ADDR;
	drv_data->vl53l0x.i2c = config->i2c.bus;

#ifdef CONFIG_VL53L0X_RECONFIGURE_ADDRESS
	if (!config->xshut.port) {
		LOG_ERR("[%s] Missing XSHUT gpio spec", dev->name);
		return -ENOTSUP;
	}
#else
	if (config->i2c.addr != VL53L0X_INITIAL_ADDR) {
		LOG_ERR("[%s] Invalid device address (should be 0x%X or "
			"CONFIG_VL53L0X_RECONFIGURE_ADDRESS should be enabled)",
			dev->name, VL53L0X_INITIAL_ADDR);
		return -ENOTSUP;
	}
#endif

	if (config->xshut.port) {
		r = gpio_pin_configure_dt(&config->xshut, GPIO_OUTPUT);
		if (r < 0) {
			LOG_ERR("[%s] Unable to configure GPIO as output",
				dev->name);
		}
	}

#ifdef CONFIG_VL53L0X_RECONFIGURE_ADDRESS
	/* Pull XSHUT low to shut down the sensor for now */
	r = gpio_pin_set_dt(&config->xshut, 0);
	if (r < 0) {
		LOG_ERR("[%s] Unable to shutdown sensor", dev->name);
		return -EIO;
	}
	LOG_DBG("[%s] Shutdown", dev->name);
#else
	r = vl53l0x_start(dev);
	if (r) {
		return r;
	}
#endif

	LOG_DBG("[%s] Initialized", dev->name);
	return 0;
}

#define VL53L0X_INIT(inst)						 \
	static struct vl53l0x_config vl53l0x_##inst##_config = {	 \
		.i2c = I2C_DT_SPEC_INST_GET(inst),			 \
		.xshut = GPIO_DT_SPEC_INST_GET_OR(inst, xshut_gpios, {}),\
		.gpio1 = GPIO_DT_SPEC_INST_GET_OR(inst, gpio1_gpios, {}) \
	};								 \
									 \
	static struct vl53l0x_data vl53l0x_##inst##_driver;		 \
									 \
	DEVICE_DT_INST_DEFINE(inst, vl53l0x_init, NULL,			 \
			      &vl53l0x_##inst##_driver,			 \
			      &vl53l0x_##inst##_config,			 \
			      POST_KERNEL,				 \
			      CONFIG_SENSOR_INIT_PRIORITY,		 \
			      &vl53l0x_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(VL53L0X_INIT)
