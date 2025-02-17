/*
 * Copyright (c) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ARCH_RISCV_ARCH_INLINES_H_
#define ZEPHYR_INCLUDE_ARCH_RISCV_ARCH_INLINES_H_

#ifndef _ASMLANGUAGE

#include <zephyr/kernel_structs.h>

static ALWAYS_INLINE _cpu_t *arch_curr_cpu(void)
{
#ifdef CONFIG_SMP
	uint32_t hartid;

	__asm__ volatile("csrr %0, mhartid" : "=r" (hartid));

	return &_kernel.cpus[hartid];
#else
	return &_kernel.cpus[0];
#endif /* CONFIG_SMP */
}

static ALWAYS_INLINE uint32_t arch_proc_id(void)
{
	/*
	 * Placeholder implementation to be replaced with an architecture
	 * specific call to get processor ID
	 */
	return arch_curr_cpu()->id;
}

#endif /* !_ASMLANGUAGE */
#endif /* ZEPHYR_INCLUDE_ARCH_RISCV_ARCH_INLINES_H_ */
