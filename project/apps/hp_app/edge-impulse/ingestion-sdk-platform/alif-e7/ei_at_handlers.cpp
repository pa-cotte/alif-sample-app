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

#include "ei_at_handlers.h"
#include "firmware-sdk/at-server/ei_at_command_set.h"
#include "firmware-sdk/ei_device_lib.h"
#include "firmware-sdk/ei_image_lib.h"
#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "peripheral/ei_uart.h"
#include "inference/ei_run_impulse.h"
#include "lcd_task.h"
#include <cmath>
#include "at_base64_lib.h"
#include "peripheral/ei_uart.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "common_events.h"
#include "board.h"

EiDeviceAlif *pei_device;

/* Private function declaration */
static bool at_list_config(void);
static bool at_clear_config(void);

static bool at_device_info(void);
static bool at_sample_start(const char **argv, const int argc);
static bool at_get_sample_settings(void);
static bool at_set_sample_settings(const char **argv, const int argc);
static bool at_get_upload_settings(void);
static bool at_set_upload_settings(const char **argv, const int argc);
static bool at_get_upload_host(void);
static bool at_set_upload_host(const char **argv, const int argc);

static bool at_read_buffer(const char **argv, const int argc);
bool at_unlink_file(const char **argv, const int argc);
static bool at_run_nn_normal(void);
static bool at_run_nn_normal_cont(void);
static bool at_run_impulse_debug(const char **argv, const int argc);
static bool at_run_nn_normal(void);
static bool at_run_nn_normal_cont(void);
static bool at_run_impulse_debug(const char **argv, const int argc);

static bool at_get_mgmt_settings(void);
static bool at_set_mgmt_settings(const char **argv, const int argc);

static bool at_get_snapshot(void);
#if !defined (BOARD_IS_ALIF_DEVKIT_E1C_VARIANT) // not E1C
static bool at_take_snapshot(const char **argv, const int argc);
static bool at_snapshot_stream(const char **argv, const int argc);
#endif

static inline bool check_args_num(const int &required, const int &received);
static bool local_read_encode_send_sample_buffer(size_t address, size_t length);

/* Public function definition */
/**
 *
 * @return
 */
ATServer *ei_at_init(EiDeviceAlif *ei_device)
{
    ATServer *at;

    at = ATServer::get_instance();
    pei_device = ei_device;

    at->register_command(AT_CONFIG, AT_CONFIG_HELP_TEXT, nullptr, at_list_config, nullptr, nullptr);
    at->register_command(AT_CLEARCONFIG, AT_CLEARCONFIG_HELP_TEXT, at_clear_config, nullptr, nullptr, nullptr);
    at->register_command(AT_SAMPLESTART, AT_SAMPLESTART_HELP_TEXT, nullptr, nullptr, at_sample_start, AT_SAMPLESTART_ARGS);
    at->register_command(AT_SAMPLESETTINGS, AT_SAMPLESETTINGS_HELP_TEXT, nullptr, at_get_sample_settings, at_set_sample_settings, AT_SAMPLESETTINGS_ARGS);
    at->register_command(AT_RUNIMPULSE, AT_RUNIMPULSE_HELP_TEXT, at_run_nn_normal, nullptr, nullptr, nullptr);
    at->register_command(AT_RUNIMPULSEDEBUG, AT_RUNIMPULSEDEBUG_HELP_TEXT, nullptr, nullptr, at_run_impulse_debug, AT_RUNIMPULSEDEBUG_ARGS);
    at->register_command(AT_RUNIMPULSECONT, AT_RUNIMPULSECONT_HELP_TEXT, at_run_nn_normal_cont, nullptr, nullptr, nullptr);
    at->register_command(AT_READBUFFER, AT_READBUFFER_HELP_TEXT, nullptr, nullptr, at_read_buffer, AT_READBUFFER_ARGS);
    at->register_command(AT_MGMTSETTINGS, AT_MGMTSETTINGS_HELP_TEXT, nullptr, at_get_mgmt_settings, at_set_mgmt_settings, AT_MGMTSETTINGS_ARGS);
    at->register_command(AT_UPLOADSETTINGS, AT_UPLOADSETTINGS_HELP_TEXT, nullptr, at_get_upload_settings, at_set_upload_settings, AT_UPLOADSETTINGS_ARGS);
    at->register_command(AT_UNLINKFILE, AT_UNLINKFILE_HELP_TEXT, nullptr, nullptr, at_unlink_file, AT_UNLINKFILE_ARGS);
    at->register_command(AT_UPLOADHOST, AT_UPLOADHOST_HELP_TEXT, nullptr, at_get_upload_host, at_set_upload_host, AT_UPLOADHOST_ARGS);
#if !defined (BOARD_IS_ALIF_DEVKIT_E1C_VARIANT) // not E1C
    at->register_command(AT_SNAPSHOT, AT_SNAPSHOT_HELP_TEXT, nullptr, at_get_snapshot, at_take_snapshot, AT_SNAPSHOT_ARGS);
    at->register_command(AT_SNAPSHOTSTREAM, AT_SNAPSHOTSTREAM_HELP_TEXT, nullptr, nullptr, at_snapshot_stream, AT_SNAPSHOTSTREAM_ARGS);
#else
    at->register_command(AT_SNAPSHOT, AT_SNAPSHOT_HELP_TEXT, nullptr, at_get_snapshot, nullptr, AT_SNAPSHOT_ARGS);
#endif

    return at;
}

/**
 *
 * @return
 */
static bool at_list_config(void)
{
    const ei_device_sensor_t *sensor_list;
    size_t sensor_list_size;

    pei_device->get_sensor_list((const ei_device_sensor_t **)&sensor_list, &sensor_list_size);

    ei_printf("===== Device info =====\n");
    at_device_info();
    ei_printf("\r\n");
    ei_printf("===== Sensors ======\r\n");
    for (size_t ix = 0; ix < sensor_list_size; ix++) {
        ei_printf(
            "Name: %s, Max sample length: %hus, Frequencies: [",
            sensor_list[ix].name,
            sensor_list[ix].max_sample_length_s);
        for (size_t fx = 0; fx < EI_MAX_FREQUENCIES; fx++) {
            if (sensor_list[ix].frequencies[fx] != 0.0f) {
                if (fx != 0) {
                    ei_printf(", ");
                }
                ei_printf_float(sensor_list[ix].frequencies[fx]);
                ei_printf("Hz");
            }
        }
        ei_printf("]\n");
    }
    ei_built_sensor_fusion_list();
    ei_printf("\r\n");
    ei_printf("===== Snapshot ======\r\n");
    at_get_snapshot();
    ei_printf("\r\n");
    ei_printf("===== Inference ======\r\n");
    ei_printf("Sensor:           %d\r\n", EI_CLASSIFIER_SENSOR);
#if EI_CLASSIFIER_OBJECT_DETECTION
    #if EI_CLASSIFIER_OBJECT_DETECTION_LAST_LAYER == EI_CLASSIFIER_LAST_LAYER_FOMO
        const char *model_type = "constrained_object_detection";
    #else
        const char *model_type = "object_detection";
    #endif
#else
    const char *model_type = "classification";
#endif
    ei_printf("Model type:       %s\r\n", model_type);
    ei_printf("\r\n");
    ei_printf("===== WIFI =====\r\n");
    ei_printf("SSID:      \r\n");
    ei_printf("Password:  \r\n");
    ei_printf("Security:  0\r\n");
    ei_printf("MAC:       00:00:00:00:00:00\r\n");
    ei_printf("Connected: 0\r\n");
    ei_printf("Present:   0\r\n");
    ei_printf("\r\n");
    ei_printf("===== Sampling parameters =====\r\n");
    at_get_sample_settings();
    ei_printf("\r\n");
    ei_printf("===== Upload settings =====\r\n");
    at_get_upload_settings();
    ei_printf("\r\n");
    ei_printf("===== Remote management =====\r\n");
    at_get_mgmt_settings();
    ei_printf("\r\n");

    return true;
}

/**
 *
 * @return
 */
static bool at_device_info(void)
{
    bool ret_val = false;

    if (pei_device != nullptr) {
        ei_printf("ID:         %s\r\n", pei_device->get_device_id().c_str());
        ei_printf("Type:       %s\r\n", pei_device->get_device_type().c_str());
        ei_printf("AT Version: %s\r\n", AT_COMMAND_VERSION);
        ei_printf("Data Transfer Baudrate: %lu\r\n", MAX_BAUD);
        ret_val = true;
    }
    else {

    }

    return ret_val;
}

/**
 *
 * @return
 */
static bool at_clear_config(void)
{
    bool ret_val = false;

    if (pei_device != nullptr) {
        ei_printf("Clearing config and restarting system...\r\n");
        pei_device->clear_config();
        //pei_device->init_device_id(); // done in clear config
        ret_val = true;
    }
    else {

    }

    return ret_val;
}

/**
 *
 * @return
 */
static bool at_get_sample_settings(void)
{
    bool ret_val = false;

    if (pei_device != nullptr) {
        ei_printf("Label:     %s\r\n", pei_device->get_sample_label().c_str());
        ei_printf("Interval:  ");
        ei_printf_float(pei_device->get_sample_interval_ms());
        ei_printf(" ms.\r\n");
        ei_printf("Length:    %lu ms.\r\n", pei_device->get_sample_length_ms());
        ei_printf("HMAC key:  %s\r\n", pei_device->get_sample_hmac_key().c_str());
        ret_val = true;
    }
    else {

    }

    return ret_val;
}

/**
 * @brief Handler for RUNIMPULE
 *
 * @return
 */
static bool at_run_nn_normal(void)
{
    ei_start_impulse(false, false, false);

    return (is_inference_running());
}

/**
 * @brief
 *
 * @param argv
 * @param argc
 * @return true
 * @return false
 */
static bool at_run_impulse_debug(const char **argv, const int argc)
{
    bool use_max_uart_speed = false;

    if (argc > 0 && argv[0][0] == 'y') {
        use_max_uart_speed = true;
    }

    ei_start_impulse(false, true, use_max_uart_speed);
    return (is_inference_running());
}

/**
 * @brief Handler for RUNIMPULSECONT
 *
 * @return
 */
static bool at_run_nn_normal_cont(void)
{
    ei_start_impulse(true, false, false);

    return (is_inference_running());
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_read_buffer(const char **argv, const int argc)
{
    bool success = false;

    if(argc < 2) {
        ei_printf("Missing argument! Required: " AT_READBUFFER_ARGS "\r\n");
        return true;
    }

    if (pei_device != nullptr)
    {
        size_t start = (size_t)atoi(argv[0]);
        size_t length = (size_t)atoi(argv[1]);

        bool use_max_baudrate = false;
        if (argc >= 3 && argv[2][0] == 'y') {
           use_max_baudrate = true;
        }

        if (use_max_baudrate) {
            ei_printf("OK\r\n");
            ei_sleep(100);
            pei_device->set_max_data_output_baudrate();
            ei_sleep(100);
        }

        success = local_read_encode_send_sample_buffer(start, length);

        if (use_max_baudrate) {
            ei_printf("\r\nOK\r\n");
            ei_sleep(100);
            pei_device->set_default_data_output_baudrate();
            ei_sleep(100);
        }

        if (!success) {
            ei_printf("Failed to read from buffer\r\n");
        }
        else {
            ei_printf("\r\n");
        }
    }

    return success;
}

bool at_unlink_file(const char **argv, const int argc)
{
    ei_printf("\n");

    return true;
}

/**
 *
 * @return
 */
static bool at_get_mgmt_settings(void)
{
    ei_printf("URL:        %s\r\n", pei_device->get_management_url().c_str());
    ei_printf("Connected:  0\r\n");
    ei_printf("Last error: \r\n");

    return true;
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_set_mgmt_settings(const char **argv, const int argc)
{
    bool ret_val = false;

    if (check_args_num(1, argc) == false) {
        return true;
    }

    if (pei_device != nullptr) {
        pei_device->set_management_url(argv[0]);
        ei_printf("OK\n");

        ret_val = true;
    }

    return ret_val;
}

/**
 *
 * @return
 */
static bool at_get_upload_settings(void)
{
    bool ret_val = false;

    if (pei_device != nullptr) {
        ei_printf("Api Key:   %s\r\n", pei_device->get_upload_api_key().c_str());
        ei_printf("Host:      %s\r\n", pei_device->get_upload_host().c_str());
        ei_printf("Path:      %s\r\n", pei_device->get_upload_path().c_str());

        ret_val = true;
    }
    else {

    }

    return ret_val;
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_set_upload_settings(const char **argv, const int argc)
{
    bool ret_val = false;

    if (check_args_num(2, argc) == false) {
        return false;
    }
    if (pei_device != nullptr) {
        pei_device->set_upload_api_key(argv[0]);
        pei_device->set_upload_host(argv[1]);

        ret_val = true;
    }

    ei_printf("OK\r\n");

    return ret_val;
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_sample_start(const char **argv, const int argc)
{
    EventBits_t event_bit;    

    if(argc < 1) {
        ei_printf("Missing sensor name!\n");
        return true;
    }

    const ei_device_sensor_t *sensor_list;
    size_t sensor_list_size;

#if (defined(LCD_SUPPORTED) && (LCD_SUPPORTED == 1)) && (defined(EI_CLASSIFIER_SENSOR) && (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA))
    bool camera_task_was_running = false;
    if (camera_task_is_running()) {
        camera_task_was_running = true;
        // stop LCD if was running
        xEventGroupSetBits(common_event_group, EVENT_STOP_LCD);

        event_bit = xEventGroupWaitBits(common_event_group, 
            EVENT_STOP_CAMERA,    //  uxBitsToWaitFor 
            pdTRUE,                 //  xClearOnExit
            pdFALSE,                //  xWaitForAllBits
            portMAX_DELAY);
    }
#endif

    pei_device->get_sensor_list((const ei_device_sensor_t **)&sensor_list, &sensor_list_size);

    for (size_t ix = 0; ix < sensor_list_size; ix++) {
        if (strcmp(sensor_list[ix].name, argv[0]) == 0) {
            if (!sensor_list[ix].start_sampling_cb()) {
                ei_printf("ERR: Failed to start sampling\n");
            }
            return true;
        }
    }

    if (ei_connect_fusion_list(argv[0], SENSOR_FORMAT)) {
        if (!ei_fusion_setup_data_sampling()) {
            ei_printf("ERR: Failed to start sensor fusion sampling\n");
        }
    }
    else {
        ei_printf("ERR: Failed to find sensor '%s' in the sensor list\n", argv[0]);
    }
    
#if (defined(LCD_SUPPORTED) && (LCD_SUPPORTED == 1)) && (defined(EI_CLASSIFIER_SENSOR) && (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA))
    if (camera_task_was_running) {
        xEventGroupSetBits(common_event_group, EVENT_START_CAMERA);
    }
#endif

    return true;
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_set_sample_settings(const char **argv, const int argc)
{
    if ((argc < 3) || (pei_device == nullptr)) {
        ei_printf("Missing argument! Required: " AT_SAMPLESETTINGS_ARGS "\r\n");
        return false;
    }

    pei_device->set_sample_label(argv[0]);

    //TODO: sanity check and/or exception handling
    std::string interval_ms_str(argv[1]);
    pei_device->set_sample_interval_ms(stof(interval_ms_str));

    //TODO: sanity check and/or exception handling
    std::string sample_length_str(argv[2]);
    pei_device->set_sample_length_ms(stoi(sample_length_str));

    if(argc >= 4) {
        pei_device->set_sample_hmac_key(argv[3]);
    }

    ei_printf("OK\r\n");

    return true;
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_set_upload_host(const char **argv, const int argc)
{
    bool ret_val = false;

    if (check_args_num(1, argc) == false) {
        return false;
    }

    if (pei_device != nullptr) {
        pei_device->set_upload_host(argv[0]);
        ret_val = true;
    }

    ei_printf("OK\r\n");

    return ret_val;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
static bool at_get_upload_host(void)
{
    if (pei_device != nullptr) {
        ei_printf("%s\n", pei_device->get_upload_host().c_str());
    }
    
    return true;
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_read_raw(const char **argv, const int argc)
{
    if(argc < 2) {
        ei_printf("Missing argument! Required: " AT_READBUFFER_ARGS "\r\n");
        return true;
    }

    volatile uint32_t start = (uint32_t)atoi(argv[0]);
    volatile uint32_t length = (uint32_t)atoi(argv[1]);

    unsigned char buffer[32];

    for(; (start < length); start += 32)
    {
        //pei_device->read_raw(buffer, start, 32);

        int n_display_bytes = (length - start) < 32 ? (length - start) : 32;
        for(int i=0; i<n_display_bytes; i++)
        {
            ei_printf("%.2X ", (unsigned char)buffer[i]);
        }
        ei_printf("\b\r\n");

        if (start > length)
            return true;
    }

    return true;
}

/**
 *
 * @return
 */
static bool at_get_snapshot(void)
{
    EiSnapshotProperties props;

    props = pei_device->get_snapshot_list();

    ei_printf("Has snapshot:         %d\r\n", props.has_snapshot ? 1 : 0);
    ei_printf("Supports stream:      %d\r\n", props.support_stream ? 1 : 0);

    if (props.has_snapshot || props.support_stream) {   // only if one of the 2
        //TODO: what is the correct format?
        ei_printf("Color depth:          %s\r\n", props.color_depth.c_str());
        ei_printf("Resolutions:          [ ");
        for (int i = 0; i < props.resolutions_num; i++) {
            ei_printf("%ux%u", props.resolutions[i].width, props.resolutions[i].height);
            if (i != props.resolutions_num - 1) {
                ei_printf(", ");
            }
        }
        ei_printf(" ]\r\n");
    }

    return true;
}
#if !defined (BOARD_IS_ALIF_DEVKIT_E1C_VARIANT) // not E1C
/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_take_snapshot(const char **argv, const int argc)
{
    uint16_t width;
    uint16_t height;
    bool use_max_baudrate = false;
    EventBits_t event_bit;    

    EiAlifCamera *cam = static_cast<EiAlifCamera*>(EiCamera::get_camera());

    if (cam->is_camera_present() == false) {
        ei_printf("ERR: camera is missing!\n");
        return false;
    }

    if (argc < 2) {
        ei_printf("Width and height arguments missing!\r\n");
        return true;
    }
    width = atoi(argv[0]);
    height = atoi(argv[1]);

    if (argc >= 3 && argv[2][0] == 'y') {
        use_max_baudrate = true;
    }

#if (defined(LCD_SUPPORTED) && (LCD_SUPPORTED == 1)) && (defined(EI_CLASSIFIER_SENSOR) && (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA))
    bool camera_task_was_running = false;
    if (camera_task_is_running()) {
        camera_task_was_running = true;
        // stop LCD if was running
        xEventGroupSetBits(common_event_group, EVENT_STOP_LCD);

        event_bit = xEventGroupWaitBits(common_event_group, 
            EVENT_STOP_CAMERA,    //  uxBitsToWaitFor 
            pdTRUE,                 //  xClearOnExit
            pdFALSE,                //  xWaitForAllBits
            portMAX_DELAY);
    }
#endif

    if (ei_camera_take_snapshot_output_on_serial(width, height, use_max_baudrate) == false) {
        ei_printf("ERR: Failed to take snapshot!\r\n");
    }

#if (defined(LCD_SUPPORTED) && (LCD_SUPPORTED == 1)) && (defined(EI_CLASSIFIER_SENSOR) && (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA))
    if (camera_task_was_running) {
        xEventGroupSetBits(common_event_group, EVENT_START_CAMERA);
    }
#endif

    return true;
}

/**
 *
 * @param argv
 * @param argc
 * @return
 */
static bool at_snapshot_stream(const char **argv, const int argc)
{
    uint32_t width, height;
    bool use_max_baudrate = false;
    EiAlifCamera *cam = static_cast<EiAlifCamera*>(EiCamera::get_camera());
    EventBits_t event_bit;

    if (cam->is_camera_present() == false) {
        ei_printf("ERR: camera is missing!\n");
        return false;
    }

    EiSnapshotProperties props;

    if(argc < 2) {
        ei_printf("Width and height arguments missing!\r\n");
        return true;
    }
    width = atoi(argv[0]);
    height = atoi(argv[1]);

    if(argc >=3 && argv[2][0] == 'y') {
        use_max_baudrate = true;
    }

#if (defined(LCD_SUPPORTED) && (LCD_SUPPORTED == 1)) && (defined(EI_CLASSIFIER_SENSOR) && (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA))
    bool camera_task_was_running = false;
    if (camera_task_is_running()) {
        camera_task_was_running = true;
        // stop LCD if was running
        xEventGroupSetBits(common_event_group, EVENT_STOP_LCD);

        event_bit = xEventGroupWaitBits(common_event_group, 
            EVENT_STOP_CAMERA,    //  uxBitsToWaitFor 
            pdTRUE,                 //  xClearOnExit
            pdFALSE,                //  xWaitForAllBits
            portMAX_DELAY);
    }
#endif

    // start stream
    if (ei_camera_start_snapshot_stream(width, height, use_max_baudrate) == false) {
        return true;
    }

    ei_printf("Snapshot streaming stopped by user\n");
    ei_printf("OK\n");

    ei_flush_rx_buffer();

#if (defined(LCD_SUPPORTED) && (LCD_SUPPORTED == 1)) && (defined(EI_CLASSIFIER_SENSOR) && (EI_CLASSIFIER_SENSOR == EI_CLASSIFIER_SENSOR_CAMERA))
    if (camera_task_was_running) {
        xEventGroupSetBits(common_event_group, EVENT_START_CAMERA);
    }
#endif

    // we do not print a new prompt!
    return true;
}
#endif

/**
 *
 * @param required
 * @param received
 * @return
 */
static inline bool check_args_num(const int &required, const int &received)
{
    if (received < required) {
        ei_printf("Too few arguments! Required: %d\r\n", required);
        return false;
    }

    return true;
}

/**
 * @brief 
 * 
 * @param address 
 * @param length 
 * @return true 
 * @return false 
 */
static bool local_read_encode_send_sample_buffer(size_t address, size_t length)
{    
    EiDeviceMemory* mem = pei_device->get_memory();    

    // we are encoiding data into base64, so it needs to be divisible by 3
    const int buffer_size = 513;
    uint8_t* buffer = (uint8_t*)ei_malloc(buffer_size);

    size_t output_size_check = floor(buffer_size / 3 * 4);
    size_t mod = buffer_size % 3;
    uint32_t size_out = 0;

    output_size_check += mod;
    uint8_t* buffer_out = (uint8_t*)ei_malloc(output_size_check);

    while (1) {
        size_t bytes_to_read = buffer_size;

        if (bytes_to_read > length) {
            bytes_to_read = length;
        }

        if (bytes_to_read == 0) {
            ei_free(buffer);
            ei_free(buffer_out);

            return true;
        }

        if (mem->read_sample_data(buffer, address, bytes_to_read) != bytes_to_read) {
            ei_free(buffer);
            ei_free(buffer_out);

            return false;
        }

        size_out = base64_encode_buffer((char *)buffer, bytes_to_read, (char *)buffer_out, output_size_check);
        ei_uart_send((char*)buffer_out, size_out);

        address += bytes_to_read;
        length -= bytes_to_read;
    }

    return true;
}
