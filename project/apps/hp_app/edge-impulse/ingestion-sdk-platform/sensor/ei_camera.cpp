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

#include "ei_camera.h"
#include "ei_classifier_porting.h"
#include "peripheral/camera/camera.h"
#include "board.h"

ei_device_snapshot_resolutions_t EiAlifCamera::resolutions[] = {
        {96, 96},
        {160, 120},
        {160, 160},
        {224, 224},
        {240, 240},
#if defined (CORE_M55_HP)
        {320, 240},
    #if (RTE_MT9M114_CAMERA_SENSOR_MIPI_IMAGE_CONFIG == 5)
            {320, 320},
    #elif (RTE_MT9M114_CAMERA_SENSOR_MIPI_IMAGE_CONFIG == 3)
        {320, 320},
        //{640, 480},
    #elif (RTE_MT9M114_CAMERA_SENSOR_MIPI_IMAGE_CONFIG == 2)
        {320, 320},
        //{640, 480},
        //{1280, 720},
    #endif
#endif

};

/**
 * @brief 
 * 
 * @param width 
 * @param height 
 * @return true 
 * @return false 
 */
bool EiAlifCamera::init(uint16_t width, uint16_t height)
{
    int retval = camera_init();
    if (retval != 0) {
        ei_printf("Failed to init camera, error code %d\r\n", retval);
        return false;
    }

    camera_found = true;

    this->width = width;
    this->height = height;

    return true;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool EiAlifCamera::deinit(void)
{
    return true;
}

/**
 * @brief 
 * 
 * @param res 
 * @param res_num 
 */
void EiAlifCamera::get_resolutions(ei_device_snapshot_resolutions_t **res, uint8_t *res_num)
{
    if (camera_found) {
        *res = &EiAlifCamera::resolutions[0];
        *res_num = sizeof(EiAlifCamera::resolutions) / sizeof(ei_device_snapshot_resolutions_t);
    }
    else {
        *res = NULL;
        *res_num = 0;
    }
    
}

/**
 * @brief 
 * 
 * @param res 
 * @return true 
 * @return false 
 */
bool EiAlifCamera::set_resolution(const ei_device_snapshot_resolutions_t res)
{
    bool retval = false;
    
    if (camera_found) {
        this->width = res.width;
        this->height = res.height;
        retval = true;
    }

    return retval;
}

bool EiAlifCamera::ei_camera_capture_rgb888_packed_big_endian(
    uint8_t *image,
    uint32_t image_size)
{
    if (!camera_found) {
        return false;
    }

    return ((camera_capture_frame(image, this->width, this->height, false) * 3) == image_size);
}

/**
 * @brief 
 * 
 * @return ei_device_snapshot_resolutions_t 
 */
ei_device_snapshot_resolutions_t EiAlifCamera::get_min_resolution(void)
{
    return EiAlifCamera::resolutions[0];
}

/**
 * @brief 
 * 
 * @param required_width 
 * @param required_height 
 * @return ei_device_snapshot_resolutions_t 
 */
ei_device_snapshot_resolutions_t EiAlifCamera::search_resolution(uint32_t required_width, uint32_t required_height)
{
    ei_device_snapshot_resolutions_t res;
    uint16_t max_width;
    uint16_t max_height;

    camera_get_max_res(&max_width, &max_height);

    if ((required_width <= max_width) && (required_height <= max_height)) {
        res.height = required_height;
        res.width = required_width;
    }
    else {
        res.height = max_height;
        res.width = max_width;
    }

    return res;
}

/**
 *
 * @return
 */
EiCamera* EiCamera::get_camera(void)
{
    static EiAlifCamera cam;

    return &cam;
}
