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

/* Includes ---------------------------------------------------------------- */
#include "ei_device_alif_e7.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "ei_utils.h"
#include "peripheral/ei_uart.h"
#include "ingestion-sdk-platform/sensor/ei_microphone.h"
#include "peripheral/timer.h"
#include "se_services_port.h"

/* Private variables ------------------------------------------------------- */

extern uint32_t        se_services_s_handle;

/** Data Output Baudrate */
const ei_device_data_output_baudrate_t ei_dev_max_data_output_baudrate = {
    ei_xstr(MAX_BAUD),
    MAX_BAUD,
};

const ei_device_data_output_baudrate_t ei_dev_default_data_output_baudrate = {
    ei_xstr(DEFAULT_BAUD),
    DEFAULT_BAUD,
};

EiDeviceAlif::EiDeviceAlif(EiDeviceMemory* mem)
{
    EiDeviceInfo::memory = mem;

    init_device_id();
    load_config();

#if defined (BOARD_IS_ALIF_DEVKIT_E1C_VARIANT)
    /* Set device type */
    device_type = "ALIF_E1C_DEVKIT";
#else
#if defined (CORE_M55_HE)
#if defined (BOARD_IS_ALIF_APPKIT_B1_VARIANT)
    device_type = "ALIF_E7_APPKIT_GEN2_HE";
#else
    device_type = "ALIF_E7_DEVKIT_GEN2_HE";
#endif
#else
#if defined (BOARD_IS_ALIF_APPKIT_B1_VARIANT)
    device_type = "ALIF_E7_APPKIT_GEN2_HP";
#else
    device_type = "ALIF_E7_DEVKIT_GEN2_HP";
#endif
#endif
#endif

    /* Init standalone sensor */
    sensors[MICROPHONE].name = "Microphone";
    sensors[MICROPHONE].frequencies[0] = 16000.0;
    sensors[MICROPHONE].max_sample_length_s = 10;
    sensors[MICROPHONE].start_sampling_cb = ei_microphone_sample_start_mono;

    sensors[MICROPHONE_2CH].name = "Microphone 2 channels";
    sensors[MICROPHONE_2CH].frequencies[0] = 16000.0;
    sensors[MICROPHONE_2CH].max_sample_length_s = 5;
    sensors[MICROPHONE_2CH].start_sampling_cb = ei_microphone_sample_start_stereo;

#if !defined (BOARD_IS_ALIF_DEVKIT_E1C_VARIANT)
    /* Init camera instance */
    cam = static_cast<EiAlifCamera*>(EiCamera::get_camera());
#endif
}

EiDeviceAlif::~EiDeviceAlif()
{
}

/**
 * @brief 
 * 
 */
void EiDeviceAlif::init_device_id(void)
{
    char temp[20];
    
    uint64_t id = 0;
    uint32_t error_code = SERVICES_REQ_SUCCESS;
    uint32_t service_error_code;

    uint8_t eui[5] = {0x04, 0x03, 0x02, 0x01, 0x00};

    SERVICES_system_get_eui_extension(se_services_s_handle, false, eui, &service_error_code);

#if defined (CORE_M55_HE)
    eui[0] = eui[0] ^ 0x01; // to differentiate between HE and HP
#endif

    snprintf(temp, sizeof(temp), "%02x:%02x:%02x:%02x:%02x",
        eui[4],
        eui[3],
        eui[2],
        eui[1],
        eui[0]);
    device_id = std::string(temp);
}

/**
 * @brief      Set output baudrate to max
 *
 */
void EiDeviceAlif::set_max_data_output_baudrate(void)
{
    ei_uart_init(MAX_BAUD);
}

/**
 * @brief      Set output baudrate to default
 *
 */
void EiDeviceAlif::set_default_data_output_baudrate(void)
{
    ei_uart_init(DEFAULT_BAUD);
}

#if ! defined (BOARD_IS_ALIF_DEVKIT_E1C_VARIANT)
/**
 * @brief      Create resolution list for snapshot setting
 *             The studio and daemon require this list
 * @param      snapshot_list       Place pointer to resolution list
 * @param      snapshot_list_size  Write number of resolutions here
 *
 * @return     False if all went ok
 */
EiSnapshotProperties EiDeviceAlif::get_snapshot_list(void)
{
    ei_device_snapshot_resolutions_t *res = NULL;

    uint8_t res_num = 0;
    EiSnapshotProperties props = {
        .has_snapshot = false,
        .support_stream = false,
        .color_depth = "",
        .resolutions_num = res_num,
        .resolutions = res
    };

    if (cam->is_camera_present() == true) {
        cam->get_resolutions(&res, &res_num);
        props.has_snapshot = true;
        props.support_stream = true;        
        props.color_depth = "RGB";
        props.resolutions_num = res_num;
        props.resolutions = res;

        return props; 
    }

    return props;
}
#endif

bool EiDeviceAlif::get_sensor_list(const ei_device_sensor_t **p_sensor_list, size_t *sensor_list_size)
{
    *p_sensor_list = sensors;
    *sensor_list_size = ARRAY_LENGTH(sensors);
    return true;
}

/**
 * @brief get_device is a static method of EiDeviceInfo class
 * It is used to implement singleton paradigm, so we are returning
 * here pointer always to the same object (dev)
 * 
 * @return EiDeviceInfo* 
 */
EiDeviceInfo* EiDeviceInfo::get_device(void)
{
#if defined (BOARD_IS_ALIF_DEVKIT_E1C_VARIANT)
    const uint32_t mem_dev_size = 0x10000;
#else
    const uint32_t mem_dev_size = 0x40000; // 256KB
#endif
    __attribute__((aligned(32), section(".bss.device_info")))  static EiDeviceRAM<262144, 4> memory(sizeof(EiConfig));
    static EiDeviceAlif dev(&memory);

    return &dev;
}

bool EiDeviceAlif::start_sample_thread(void (*sample_read_cb)(void), float sample_interval_ms)
{
    this->is_sampling = true;
    this->sample_read_callback = sample_read_cb;
    this->sample_interval_ms = sample_interval_ms;

#if MULTI_FREQ_ENABLED == 1
    this->actual_timer = 0;
    this->fusioning = 1;        //
#endif
    
    timer_sensor_start(this->sample_interval_ms);

    return true;
}

bool EiDeviceAlif::stop_sample_thread(void)
{
    timer_sensor_stop();

    this->set_state(eiStateIdle);
    this->is_sampling = false;

    return true;
}

void EiDeviceAlif::sample_thread(void)
{       
#if MULTI_FREQ_ENABLED == 1
    if (this->fusioning == 1) {
        if (timer_sensor_get() == true)
        {
            if (this->sample_read_callback != nullptr) {
                this->sample_read_callback();
            }
        }
    }
    else {
        uint8_t flag = 0;
        uint8_t i = 0;

        this->actual_timer += (uint32_t)this->sample_interval;

        if (timer_sensor_get() == true) {

            for (i = 0; i < this->fusioning; i++) {
                if (((uint32_t)(this->actual_timer % (uint32_t)this->multi_sample_interval[i])) == 0) {
                    flag |= (1<<i);
                }
            }

            if (this->sample_multi_read_callback != nullptr) {
                this->sample_multi_read_callback(flag);
            }
        }
    }
#else
    if (timer_sensor_get() == true)
    {
        if (this->sample_read_callback != nullptr) {
            this->sample_read_callback();
        }
    }

#endif
}

#if MULTI_FREQ_ENABLED == 1
/**
 *
 * @param sample_read_cb
 * @param multi_sample_interval_ms
 * @param num_fusioned
 * @return
 */
bool EiDeviceAlif::start_multi_sample_thread(void (*sample_multi_read_cb)(uint8_t), float* multi_sample_interval_ms, uint8_t num_fusioned)
{
    uint8_t i;
    uint8_t flag = 0;

    this->is_sampling = true;
    this->sample_multi_read_callback = sample_multi_read_cb;
    this->fusioning = num_fusioned;

    this->multi_sample_interval.clear();

    for (i = 0; i < num_fusioned; i++) {
        this->multi_sample_interval.push_back(1.f/multi_sample_interval_ms[i]*1000.f);
    }

    this->sample_interval = ei_fusion_calc_multi_gcd(this->multi_sample_interval.data(), this->fusioning);

    /* force first reading */
    for (i = 0; i < this->fusioning; i++) {
            flag |= (1<<i);
    }
    this->sample_multi_read_callback(flag);

    this->actual_timer = 0;
    timer_sensor_start((uint32_t)this->sample_interval);

    return true;
}
#endif
