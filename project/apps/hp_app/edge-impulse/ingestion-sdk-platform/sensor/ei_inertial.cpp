/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Include ----------------------------------------------------------------- */
#include "ei_inertial.h"
#include "peripheral/inertial/bmi323_icm42670.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"

/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2 (9.80665f)
#define ACC_RAW_SCALING  (32767.5f)

#define ACC_SCALE_FACTOR (2.0f * CONVERT_G_TO_MS2) / ACC_RAW_SCALING
#define CONVERT_ADC_GYR (float)(250.0f / 32768.0f)

/* Private variables ------------------------------------------------------- */
static float imu_data[INERTIAL_AXIS_SAMPLED];
static float imu_data_bmi323[INERTIAL_AXIS_SAMPLED];

/**
 * @brief      Setup payload header
 *
 * @return     true
 */
bool ei_inertial_init(void)
{
    int retval = bmi323_icm42670_init();
    if (retval == -1) {
        ei_printf("Failed to initialize peripheral\r\n");
        return false;
    }
    else {
        if (retval & 0x02) {
            ei_add_sensor_to_fusion_list(inertial_sensor_icm42670);
        }
        else {
            ei_printf("Failed to initialize inertial_sensor_icm42670\r\n");
        }

        if (retval & 0x01) {
            if (retval & 0x02) {
                ei_add_sensor_to_fusion_list(inertial_sensor_bmi323);   // we have both
            }
            else {
                ei_add_sensor_to_fusion_list(just_inertial_sensor_bmi323);  // just bmi
            }
            
        }
        else {
            ei_printf("Failed to initialize inertial_sensor_bmi323\r\n");
        }

        return true;
    }

    
}

/**
 * @brief
 *
 * @param n_samples
 * @return float*
 */
float *ei_fusion_inertial_read_data_icm42670(int n_samples)
{
    xyz_accel_gyro_imu_s icm_data;

    memset(imu_data, 0, sizeof(imu_data));

    getICM42670PAllSensors(&icm_data);
    imu_data[0] = (float)icm_data.acc_x * ACC_SCALE_FACTOR;
    imu_data[1] = (float)icm_data.acc_y * ACC_SCALE_FACTOR;
    imu_data[2] = (float)icm_data.acc_z * ACC_SCALE_FACTOR;

    if ((n_samples > 3)) {
        imu_data[3] = (float)icm_data.gyro_x * CONVERT_ADC_GYR;
        imu_data[4] = (float)icm_data.gyro_y * CONVERT_ADC_GYR;
        imu_data[5] = (float)icm_data.gyro_z * CONVERT_ADC_GYR;
    }

    return imu_data;
}

/**
 * @brief 
 * 
 * @param n_samples 
 * @return float* 
 */
float* ei_fusion_inertial_read_data_bmi323(int n_samples)
{
    xyz_accel_gyro_imu_s bmi_data;

    memset(imu_data_bmi323, 0, sizeof(imu_data_bmi323));

    getBMI323AllSensors(&bmi_data);
    imu_data_bmi323[0] = (float)bmi_data.acc_x * ACC_SCALE_FACTOR;
    imu_data_bmi323[1] = (float)bmi_data.acc_y * ACC_SCALE_FACTOR;
    imu_data_bmi323[2] = (float)bmi_data.acc_z * ACC_SCALE_FACTOR;

    if ((n_samples > 3)) {
        imu_data_bmi323[3] = (float)bmi_data.gyro_x * CONVERT_ADC_GYR;
        imu_data_bmi323[4] = (float)bmi_data.gyro_y * CONVERT_ADC_GYR;
        imu_data_bmi323[5] = (float)bmi_data.gyro_z * CONVERT_ADC_GYR;
    }

    return imu_data_bmi323;
}
