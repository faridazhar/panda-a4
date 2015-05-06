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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>

#include "MPU6050Sensor.h"

#define FETCH_FULL_EVENT_BEFORE_RETURN 1

/*****************************************************************************/

MPU6050Sensor::MPU6050Sensor(const char *name, int device_fd)
    : SensorBase(NULL, name, device_fd),
      mPendingMask(0),
      mEnabled(0),
      mInputReader(64),
      mHasPendingEvent(false)
{

    if (!strcmp(ACCEL_INPUT_NAME, name)) {
        mPendingEvents[accelerometer].version = sizeof(sensors_event_t);
        mPendingEvents[accelerometer].sensor = ID_A;
        mPendingEvents[accelerometer].type = SENSOR_TYPE_ACCELEROMETER;
    }else if (!strcmp(GYRO_INPUT_NAME, name)) {
        mPendingEvents[gyroscope].version = sizeof(sensors_event_t);
        mPendingEvents[gyroscope].sensor = ID_GY;
        mPendingEvents[gyroscope].type = SENSOR_TYPE_GYROSCOPE;
    } else {
        return;
    }

    if (data_fd) {
        strcpy(input_sysfs_path, "/sys/bus/i2c/drivers/mpu6050/2-0068/");
        input_sysfs_path_len = strlen(input_sysfs_path);
    }
}

MPU6050Sensor::~MPU6050Sensor() {
    if (mEnabled) {
        enable(ID_A, 0);
        enable(ID_GY, 0);
    }
}

int MPU6050Sensor::enable(int32_t handle, int en) {
   int flags = en ? 1 : 0;
    int accel_fd = -1;
    int gyro_fd = -1;

    if (handle == ID_A) {
        strcpy(&input_sysfs_path[input_sysfs_path_len], "accel_enable");
        accel_fd = open(input_sysfs_path, O_RDWR);
        if (accel_fd >= 0) {
            char buffer[2];
            sprintf(buffer, "%d\n", flags);
            write(accel_fd, buffer, sizeof(buffer));
            close(accel_fd);
            mEnabled = flags << accelerometer;
        } else {
            return -1;
        }
    } else if (handle == ID_GY) {
        strcpy(&input_sysfs_path[input_sysfs_path_len], "gyro_enable");
        gyro_fd = open(input_sysfs_path, O_RDWR);
        if (gyro_fd >= 0) {
            char buffer[2];
            sprintf(buffer, "%d\n", flags);
            write(gyro_fd, buffer, sizeof(buffer));
            close(gyro_fd);
            mEnabled = flags << gyroscope;
        } else {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

bool MPU6050Sensor::hasPendingEvents() const {
    return mHasPendingEvent;
}

int MPU6050Sensor::setDelay(int32_t handle, int64_t delay_ns)
{
    return 0;
}

int MPU6050Sensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

#if FETCH_FULL_EVENT_BEFORE_RETURN
again:
#endif
    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        float value = event->value;
        if (type == EV_ABS) {
            if (event->code == EVENT_TYPE_ACCEL_X) {
                mPendingEvents[accelerometer].data[0] = value * CONVERT_A_X;
            } else if (event->code == EVENT_TYPE_ACCEL_Y) {
                mPendingEvents[accelerometer].data[1] = value * CONVERT_A_Y;
            } else if (event->code == EVENT_TYPE_ACCEL_Z) {
                mPendingEvents[accelerometer].data[2] = value * CONVERT_A_Z;
            }
            mPendingMask |= 1 << accelerometer;
            mInputReader.next();
       } else if (type == EV_REL) {
            if (event->code == EVENT_TYPE_GYRO_X) {
                mPendingEvents[gyroscope].data[0] = value * CONVERT_GYRO_X;
            } else if (event->code == EVENT_TYPE_GYRO_Y) {
                mPendingEvents[gyroscope].data[1] = value * CONVERT_GYRO_Y;
            } else if (event->code == EVENT_TYPE_GYRO_Z) {
                mPendingEvents[gyroscope].data[2] = value * CONVERT_GYRO_Z;
            }
            mPendingMask |= 1 << gyroscope;
            mInputReader.next();
        } else if (type == EV_SYN) {
           int64_t time = timevalToNano(event->time);
            for (int j=0 ; count && mPendingMask && j<numSensors ; j++) {
              if (mPendingMask & (1 << j)) {
                    mPendingMask &= ~(1 << j);
                    mPendingEvents[j].timestamp = time;
                    *data++ = mPendingEvents[j];
                        count--;
                        numEventReceived++;
              }
            }
            if (!mPendingMask) {
                mInputReader.next();
            }
        } else {
            ALOGE("MPU6050: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
    }

#if FETCH_FULL_EVENT_BEFORE_RETURN
    /* if we didn't read a complete event, see if we can fill and
       try again instead of returning with nothing and redoing poll. */
    if (numEventReceived == 0 && mEnabled == 1) {
        n = mInputReader.fill(data_fd);
        if (n)
            goto again;
    }
#endif

    return numEventReceived;
}
