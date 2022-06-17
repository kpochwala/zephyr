#pragma once

int vl53l0x_extra_calibrate_xtalk(const struct device *dev, int distance_mm, uint32_t *xtalk_output);
int vl53l0x_extra_save_xtalk(const struct device *dev, uint32_t xtalk_data);
int vl53l0x_extra_calibrate_offset(const struct device *dev, int distance_mm, int32_t *offset_micrometer);
int vl53l0x_extra_save_offset(const struct device *dev, int32_t offset_micrometer);
