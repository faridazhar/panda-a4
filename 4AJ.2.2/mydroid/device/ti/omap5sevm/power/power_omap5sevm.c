/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#define LOG_TAG "OMAPPowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define CPUFREQ_INTERACTIVE     "/sys/devices/system/cpu/cpufreq/interactive/"
#define BOOSTPULSE_PATH 	"/sys/devices/system/cpu/cpufreq/interactive/boostpulse"
#define MAX_SCALING_FREQ_PATH	"/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define TIMER_RATE_PATH		"/sys/devices/system/cpu/cpufreq/interactive/timer_rate"
#define MIN_SAMPLE_TIME_PATH	"/sys/devices/system/cpu/cpufreq/interactive/min_sample_time"
#define HISPEED_FREQ_PATH	"/sys/devices/system/cpu/cpufreq/interactive/hispeed_freq"
#define HISPEED_LOAD_VAL_PATH	"/sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load"
#define HISPEED_DELAY_PATH	"/sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay"
#define AVAIL_FREQ_PATH         "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies"

/*
 * Nominal frequency is the maximum frequency allowed for the
 * system to operate under specific conditions
 * For OMAP based smart phones and tablets, this condition
 * is identified as the display OFF scenario.
 * The max nominal frequency allowed is fixed at 2nd lowest
 * frequency at which the cpu can operate. The index of this
 * frequency is 1
 */
static int nomspeed_freq_idx = 1;

/*
 * Max high speed frequency is the maximum frequency supported
 * by the cpu. This frequency needs to be fetched from kernel side
 */
static int hispeed_freq_idx;

/* Tuning parameters table index definition */
#define NUM_TUNING_PARAMS       2
#define NUM_PROFILES            2

/*
 * Tuning parameters table structure
 * col_param_path  : <path to governor> <specific
 *                                       sysfs parameter>
 *                   The path defines the sysfs path for the
 *                   governor parameter
 * row_profile_name: defines a profile that consists of a
 *                   set of parameters
 * param_val       : param values for a profile
 */
struct tuning_table {
    char *col_param_path[NUM_TUNING_PARAMS];
    char *row_profile_name[NUM_PROFILES];
    char *param_val[][NUM_TUNING_PARAMS];
};

/*
 * The values below define the parameters that can
 * be changed for interactive governor.
 * These are specific to a platform
 * current table representation is follows. Values are example
 *                      go_hispeed_load  min_sample_time
 *    high_performance          40           60000
 *    low_power                 80           40000
 */
static struct tuning_table omap_tuning_table = {
        .col_param_path   = {
                             CPUFREQ_INTERACTIVE "go_hispeed_load",
                             CPUFREQ_INTERACTIVE "min_sample_time",
                            },
        .row_profile_name = {
                             "high_performance",
                             "low_power"
                            },
        .param_val        = {
                             {"40", "40000"},
                             {"70", "20000"},
                            },
};

#define HI_PERF_ROW_IDX                 0  /* Row index in tuning
                                              table for high
                                              performance profile*/
#define LOW_PWR_ROW_IDX                 1  /* Row index in tuning
                                              table for Low Power
                                              profile */

static int num_avail_freq;
#define MAX_FREQ_NUMBER 10
#define MAX_TOKEN_SIZE 9
static char *freq_list[MAX_FREQ_NUMBER];

struct omap5sevm_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
    bool ready;
};

static int str_to_tokens(char *str, char **token,
                         int max_token_idx, char *delim)
{
    char *pos, *start_pos = str;
    char *token_pos;
    int token_idx = 0;

    if (!str || !token || !max_token_idx) {
        return -EINVAL;
    }

    do {
        token_pos = strtok_r(start_pos, delim, &pos);
        if (token_pos)
            token[token_idx++] = strdup(token_pos);

        start_pos = NULL;
    } while (token_pos && token_idx < max_token_idx);

    return token_idx;
}

static int sysfs_read(char *path, void *dest, int nbytes)
{
    char buf[80];
    int len, ret = 0;
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -EBADF;
    }

    len = read(fd, dest, nbytes);
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
        ret = -EIO;
    }

    close(fd);
    return ret;
}

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static void sysfs_write_tuning_params(int profile_idx)
{
    int i, idx;
     /* set parameters for the governors */
     for (i = 0; i < NUM_TUNING_PARAMS; i++) {
         if (omap_tuning_table.param_val[profile_idx][i])
             sysfs_write(omap_tuning_table.col_param_path[i],
                         omap_tuning_table.param_val[profile_idx][i]);
     }

     if (profile_idx == HI_PERF_ROW_IDX)
         idx = hispeed_freq_idx;
     else
         idx = nomspeed_freq_idx;
     sysfs_write(MAX_SCALING_FREQ_PATH, freq_list[idx]);
}

static void omap5sevm_power_init(struct power_module *module)
{
    struct omap5sevm_power_module *omap5sevm = (struct omap5sevm_power_module *) module;
    char freq_buf[MAX_FREQ_NUMBER* MAX_TOKEN_SIZE];
    char delim[] = " \n";
    int ret;

    memset(freq_buf, '\0', sizeof(freq_buf));
    ret = sysfs_read(AVAIL_FREQ_PATH, freq_buf, sizeof(freq_buf));
    if (ret < 0) {
        ALOGI(LOG_TAG " initialization failed");
        /* governor works now with its default parameters */
        return;
    }

    num_avail_freq = str_to_tokens(freq_buf, freq_list,
                                   MAX_FREQ_NUMBER, delim);
    if (num_avail_freq < 0) {
        /* governor works now with its default parameters */
        ALOGI(LOG_TAG " initialization failed");
        return;
    }

    hispeed_freq_idx = num_avail_freq - 1;
    if (num_avail_freq == 1)
        nomspeed_freq_idx = hispeed_freq_idx;

    omap5sevm->ready = true;
    ALOGI(LOG_TAG " initialized");
}

static int boostpulse_open(struct omap5sevm_power_module *omap5sevm)
{
    char buf[80];

    pthread_mutex_lock(&omap5sevm->lock);

    if (omap5sevm->boostpulse_fd < 0) {
        omap5sevm->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);

        if (omap5sevm->boostpulse_fd < 0) {
            if (!omap5sevm->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, buf);
                omap5sevm->boostpulse_warned = 1;
            }
        }
    }

    pthread_mutex_unlock(&omap5sevm->lock);
    return omap5sevm->boostpulse_fd;
}

static void omap5sevm_power_set_interactive(struct power_module *module, int on)
{
    struct omap5sevm_power_module *omap5sevm = (struct omap5sevm_power_module *) module;
    int idx;

    if (omap5sevm->ready == false)
        return;

    idx = on ? HI_PERF_ROW_IDX : LOW_PWR_ROW_IDX;

    /*
     * Policy: Go to low power profile when screen is off.
     * Go to high performance profile when screen is on
     */
    sysfs_write_tuning_params(idx);
}

static void omap5sevm_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    struct omap5sevm_power_module *omap5sevm = (struct omap5sevm_power_module *) module;
    char buf[80];
    int len;

    switch (hint) {
    case POWER_HINT_INTERACTION:
        if (boostpulse_open(omap5sevm) >= 0) {
	    len = write(omap5sevm->boostpulse_fd, "1", 1);

	    if (len < 0) {
	        strerror_r(errno, buf, sizeof(buf));
		ALOGE("Error writing to %s: %s\n", BOOSTPULSE_PATH, buf);
	    }
	}
        break;

    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct omap5sevm_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "OMAP5sevm Power HAL",
            author: "The Android Open Source Project",
            methods: &power_module_methods,
        },

       init: omap5sevm_power_init,
       setInteractive: omap5sevm_power_set_interactive,
       powerHint: omap5sevm_power_hint,
    },

    lock: PTHREAD_MUTEX_INITIALIZER,
    boostpulse_fd: -1,
    boostpulse_warned: 0,
    ready: false,
};
