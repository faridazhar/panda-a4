/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _OMAP4_BOARD_IDENTITY_H
#define _OMAP4_BOARD_IDENTITY_H

#if __cplusplus
extern "C" {
#endif

#define SOC_FAMILY	0
#define SOC_REV		1
#define SOC_TYPE	2
#define SOC_MAX_FREQ	3
#define APPS_BOARD_REV	4
#define CPU_MAX_FREQ	5
#define CPU_GOV		6
#define LINUX_VERSION	7
#define LINUX_PVR_VER	8
#define LINUX_CMDLINE	9
#define LINUX_CPU1_STAT	10
#define LINUX_OFF_MODE	11
#define WILINK_VERSION	12
#define DPLL_TRIM	13
#define RBB_TRIM	14
#define PRODUCTION_ID	15
#define DIE_ID	        16
#define MAX_FIELDS	(DIE_ID + 1)

#define PROP_DISPLAY_ID 	0
#define PROP_BUILD_TYPE 	1
#define PROP_SER_NO		2
#define PROP_BOOTLOADER 	3
#define PROP_DEBUGGABLE 	4
#define PROP_CRYPTO_STATE 	5
#define MAX_PROP 	(PROP_CRYPTO_STATE + 1)

#define GOV_STRING "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define CPU1_ONLINE "/sys/devices/system/cpu/cpu1/online"
#define OFF_MODE "/sys/kernel/debug/pm_debug/enable_off_mode"
#define GOV_PERFORMANCE 	0
#define GOV_HOTPLUG		1
#define GOV_ONDEMAND		2
#define GOV_INTERACTIVE		3
#define GOV_USERSPACE		4
#define GOV_CONSERVATIVE	5
#define GOV_POWERSAVE		6
#define GOV_MAX			7

char* get_device_identity(int id_number);
char* get_device_property(int id_number);
int set_governor(int governor);
char* get_dpll_trim_val(void);
#if __cplusplus
}  // extern "C"
#endif

#endif  // _OMAP4_BOARD_IDENTITY_H
