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

#include "ei_microphone.h"

#include "FreeRTOS.h"
#include "event_groups.h"

#include "RTE_Components.h"
#include "RTE_Device.h"
#include "ei_classifier_porting.h"
#include <Driver_SAI.h>
#include CMSIS_device_header
#include "board.h"

#include "ingestion-sdk-platform/alif-e7/ei_device_alif_e7.h"
#include "ingestion-sdk-c/sensor_aq_mbedtls_hs256.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"
#include "firmware-sdk/sensor-aq/sensor_aq.h"

#define I2S_ADC       BOARD_I2S_INSTANCE

#define _I2S_IRQ(n)      I2S##n##_IRQ_IRQn
#define  I2S_IRQ(n)     _I2S_IRQ(n)

extern ARM_DRIVER_SAI ARM_Driver_SAI_(I2S_ADC);
ARM_DRIVER_SAI*       s_i2s_drv;

static EventGroupHandle_t mic_event;

static bool create_header(sensor_aq_payload_info *payload);
static size_t ei_write(const void*, size_t size, size_t count, EI_SENSOR_AQ_STREAM*);
static int ei_seek(EI_SENSOR_AQ_STREAM*, long int offset, int origin);
static void audio_buffer_callback(void *buffer, uint32_t n_bytes);

static int insert_ref(char *buffer, int hdrLength);
static void write_value_to_cbor_buffer(uint8_t *buf, int16_t value);
static void I2SCallback(uint32_t event);

static bool ei_microphone_init_channels(void);
static bool ei_microphone_sample_start(void);
static bool ei_microphone_start(void);
static bool ei_microphone_stop(void);

/* Private variables ------------------------------------------------------- */
#define SAMPLING_RATE               (16000u)

#define SAMPLE_SIZE_IN_MS           (100u)
#define MIC_SAMPLE_SIZE_MONO        (SAMPLING_RATE / (SAMPLE_SIZE_IN_MS * 2))
//#define MIC_SAMPLE_SIZE_STEREO      (MIC_SAMPLE_SIZE_MONO * 2)                  /* 50 ms - stereo */


//#define AUDIO_REC_SAMPLES           (512)
#define AUDIO_REC_SAMPLES           MIC_SAMPLE_SIZE_MONO
#define MIC_EVENT_RX_DONE           ARM_SAI_EVENT_RECEIVE_COMPLETE
#define MIC_EVENT_RX_OVERFLOW       ARM_SAI_EVENT_RX_OVERFLOW
#define MIC_EVENT_ERROR             ARM_SAI_EVENT_FRAME_ERROR

#define MAX_AUDIO_CHANNELS          (2)

static bool record_ready = false;
static bool is_recording = false;
static uint8_t n_audio_channels = 2;
static uint32_t cbor_current_sample;

// temp buffer to store
static int16_t mic_buffer[AUDIO_REC_SAMPLES * MAX_AUDIO_CHANNELS * 2] __attribute__((aligned(32), section(".bss.mic_buffer")));

// recording buffer
static int audio_current_rec_buf = 0;
static int32_t mic_rec[2][AUDIO_REC_SAMPLES * MAX_AUDIO_CHANNELS] __attribute__((aligned(32), section(".bss.audio_buffer")));

static uint32_t ei_mic_get_int16_from_buffer(int32_t* src, int16_t* dst, uint32_t element);

/** Status and control struct for inferencing struct */
typedef struct {
    int16_t *buffers[2];
    uint8_t buf_select;
    uint8_t buf_ready;
    uint32_t buf_count;
    uint32_t n_samples;
} inference_t;

static inference_t inference;
static uint32_t headerOffset;
static uint32_t samples_required;
static uint32_t current_sample;

static unsigned char ei_mic_ctx_buffer[1024];
static sensor_aq_signing_ctx_t ei_mic_signing_ctx;
static sensor_aq_mbedtls_hs256_ctx_t ei_mic_hs_ctx;
static sensor_aq_ctx ei_mic_ctx = {
    { ei_mic_ctx_buffer, 1024 },
    &ei_mic_signing_ctx,
    &ei_write,
    &ei_seek,
    NULL,
};

/**
 * @brief 
 * 
 * @return int 
 */
int ei_microphone_init(void)
{
    int32_t status = 0;
    /* Use the I2S as Receiver */
    s_i2s_drv = &ARM_Driver_SAI_(I2S_ADC);

    /* Verify if I2S protocol is supported */
    ARM_SAI_CAPABILITIES cap = s_i2s_drv->GetCapabilities();
    if (!cap.protocol_i2s) {
        ei_printf("I2S is not supported\n");
        return false;
    }

    /* Initializes I2S interface */
    status = s_i2s_drv->Initialize(I2SCallback);
    if (status) {
        ei_printf("I2S Initialize failed status = %d\n", status);
        return false;
    }

    /* Enable the power for I2S */
    status = s_i2s_drv->PowerControl(ARM_POWER_FULL);
    if (status) {
        ei_printf("I2S Power failed status = %d\n", status);
        s_i2s_drv->Uninitialize();
        return false;
    }

    mic_event = xEventGroupCreate();

    return true;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool ei_microphone_sample_start_mono(void)
{
    n_audio_channels = 1;
    ei_microphone_init_channels();

    return ei_microphone_sample_start();
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool ei_microphone_sample_start_stereo(void)
{
    n_audio_channels = 2;
    ei_microphone_init_channels();

    return ei_microphone_sample_start();
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
static bool ei_microphone_sample_start(void)
{
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    EiDeviceMemory *mem = dev->get_memory();

    sensor_aq_payload_info payload = {
        dev->get_device_id().c_str(),
        dev->get_device_type().c_str(),
        dev->get_sample_interval_ms(),
        { { "audio", "wav" } }
    };

    if(n_audio_channels > 1) {
        payload.sensors[1] = { "audio2", "wav"};
    }

    ei_printf("Sampling settings:\n");
    ei_printf("\tInterval: ");
    ei_printf_float(dev->get_sample_interval_ms());
    ei_printf(" ms.\n");
    ei_printf("\tLength: %lu ms.\n", dev->get_sample_length_ms());
    ei_printf("\tName: %s\n", dev->get_sample_label().c_str());
    ei_printf("\tHMAC Key: %s\n", dev->get_sample_hmac_key().c_str());
    ei_printf("\tFile name: /fs/%s\n", dev->get_sample_label().c_str());

    samples_required = (uint32_t)((dev->get_sample_length_ms()) / dev->get_sample_interval_ms());

    /* Round to even number of samples for word align flash write */
    if(samples_required & 1) {
        samples_required++;
    }

    current_sample = 0;
    cbor_current_sample = 0;

    ei_printf("Starting in 2000 ms... (or until all flash was erased)\n");
    ei_sleep(2000); // no need to erase

    if (create_header(&payload) == false) {
        return false;
    }

    // start
    ei_printf("Sampling...\r\n");
    is_recording = true;
    audio_current_rec_buf = 0;
    record_ready = false;
    dev->set_state(eiStateSampling);

    ei_microphone_start();

    while (record_ready == false) {
        ei_mic_thread(audio_buffer_callback);        
    };

    // stop
    ei_microphone_stop();

    dev->set_state(eiStateIdle);

    /* Write end of cbor + dummy */
    const uint8_t end_of_cbor[] = {0xff, 0xff, 0xff, 0xff};
    mem->write_sample_data((uint8_t*)end_of_cbor, headerOffset + cbor_current_sample, 4);

    int ctx_err =
        ei_mic_ctx.signature_ctx->finish(ei_mic_ctx.signature_ctx, ei_mic_ctx.hash_buffer.buffer);
    if (ctx_err != 0) {
        ei_printf("Failed to finish signature (%d)\n", ctx_err);
        return false;
    }

    ei_printf("Done sampling, total bytes collected: %u\n", (current_sample * 2));
    ei_printf("[1/1] Uploading file to Edge Impulse...\n");
    ei_printf("Not uploading file, not connected to WiFi. Used buffer, from=0, to=%u.\n", (cbor_current_sample + 1 + headerOffset));
    ei_printf("OK\n");

    return true;
}

/**
 * @brief 
 * 
 * @param n_samples 
 * @param interval_ms 
 * @return true 
 * @return false 
 */
bool ei_microphone_inference_start(uint32_t n_samples, uint8_t n_channels_inference, uint32_t freq)
{
    int32_t status = 0;

    if (freq != SAMPLING_RATE) {
        ei_printf("Sample rate not supported: %d, expected %d\r\n", freq, SAMPLING_RATE);
        return false;
    }

    if (n_channels_inference > 2) {
        ei_printf("Wrong number of channels\r\n");
        return false;
    }

    n_audio_channels = n_channels_inference;

    inference.buffers[0] = (int16_t *)ei_malloc(n_samples * sizeof(microphone_sample_t)  * n_audio_channels);
    if (inference.buffers[0] == NULL) {
        ei_printf("Can't allocate first audio buffer\r\n");
        return false;
    }

    inference.buffers[1] = (int16_t *)ei_malloc(n_samples * sizeof(microphone_sample_t)  * n_audio_channels);
    if (inference.buffers[1] == NULL) {
        ei_free(inference.buffers[0]);
        ei_printf("Can't allocate second audio buffer\r\n");
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;    // this represents the number of samples per channel
    inference.buf_ready = 0;

    audio_current_rec_buf = 0;

    ei_microphone_init_channels();

    return true;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool ei_microphone_inference_is_recording(void)
{
    return (inference.buf_ready == 0);
}

/**
 * @brief 
 * 
 */
void ei_microphone_inference_reset_buffers(void)
{
    inference.buf_ready = 0;
    inference.buf_count = 0;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool ei_microphone_inference_end(void)
{
    ei_microphone_stop();

    ei_free(inference.buffers[0]);
    ei_free(inference.buffers[1]);

    return true; 
}

/**
 * @brief 
 * 
 * @param offset 
 * @param length 
 * @param out_ptr 
 * @return int 
 */
int ei_microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    ei::numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
    inference.buf_ready = 0;

    return 0;
}

/**
 * @brief Callback routine from the i2s driver.
 *
 * @param[in]  event  Event for which the callback has been called.
 */
static void I2SCallback(uint32_t event)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xEventGroupSetBitsFromISR(mic_event, event, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief 
 * 
 * @param buffer 
 * @param sample_count 
 */
void ei_mic_inference_samples_callback(void *buffer, uint32_t sample_count)
{
    int16_t *sbuffer = mic_buffer;
    uint32_t sbuf_ptr = 0;

    sample_count /= n_audio_channels; // number of samples per channel
    uint32_t n_bytes = (sample_count * 2);   // in bytes after conversion to int16_t from original size of int32_t

    ei_mic_get_int16_from_buffer((int32_t*)buffer, sbuffer, sample_count);

    for (uint32_t i = 0; i < sample_count; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sbuffer[i];

        if (inference.buf_count >= (inference.n_samples * n_audio_channels)) {  // n_samples is per channel

            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }   
}

/**
 * @brief 
 * 
 * @param in_buffer 
 * @param n_samples The number of samples collected
 */
static void audio_buffer_callback(void *in_buffer, uint32_t n_samples)
{
    EiDeviceMemory *mem = EiDeviceInfo::get_device()->get_memory();
    int16_t *sbuffer = mic_buffer;
    uint32_t sbuf_ptr = 0;

    n_samples /= n_audio_channels; // number of samples per channel
    uint32_t n_bytes = (n_samples * 2);   // in bytes after conversion to int16_t from original size of int32_t

    // check if we should start recording again or we finished
    current_sample += n_samples;

    if (current_sample >= samples_required) {
        record_ready = true;
        is_recording = false;
    }
    else {
        audio_current_rec_buf = !audio_current_rec_buf;
        ei_microphone_start();  // start the next recording
    }
    
    ei_mic_get_int16_from_buffer((int32_t*)in_buffer, sbuffer, n_samples);
    
    /* Calculate the cbor buffer length: header + 3 bytes per audio channel */
    uint32_t cbor_length = n_samples + ((n_bytes + n_samples) * n_audio_channels);    
    uint8_t *cbor_buf = (uint8_t *)ei_malloc(cbor_length);

    if (cbor_buf == NULL) {
        record_ready = true;
        is_recording = false;
        ei_printf("ERR: memory allocation error\r\n");
        return;
    }

    for (int i = 0; i < n_samples; i++) {
        uint32_t cval_ptr = i * ((n_audio_channels * 3) + 1);

        cbor_buf[cval_ptr] = 0x80 + n_audio_channels;
        for (int y = 0; y < n_audio_channels; y++) {
            write_value_to_cbor_buffer(&cbor_buf[cval_ptr + 1 + (3 * y)], sbuffer[sbuf_ptr + y]);
        }
        sbuf_ptr += n_audio_channels;
    }
    

    mem->write_sample_data((uint8_t*)cbor_buf, headerOffset + cbor_current_sample, cbor_length);

    ei_free(cbor_buf);

    cbor_current_sample += cbor_length; 
}

/**
 * @brief 
 * 
 * @param buffer 
 * @param size 
 * @param count 
 * @param stream 
 * @return size_t 
 */
static size_t ei_write(const void* buffer, size_t size, size_t count, EI_SENSOR_AQ_STREAM* stream)
{
    (void)buffer;
    (void)size;
    (void)stream;

    return count;
}

/**
 * @brief 
 * 
 * @param stream 
 * @param offset 
 * @param origin 
 * @return int 
 */
static int ei_seek(EI_SENSOR_AQ_STREAM* stream, long int offset, int origin)
{
    (void)stream;
    (void)offset;
    (void)origin;

    return 0;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
static bool ei_microphone_start(void)
{
    int32_t status = 0;
    
    /* receive data */
    status = s_i2s_drv->Receive(mic_rec[audio_current_rec_buf], (AUDIO_REC_SAMPLES * n_audio_channels)); // Number of samples times number of channels
    if (status) {        
        return false;
    }

    return true;
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool ei_microphone_start_inference_recording(void)
{
    return ei_microphone_start();
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
static bool ei_microphone_stop(void)
{
    int32_t status = 0;
    
    /* Stop mic */
    status = s_i2s_drv->Control(ARM_SAI_CONTROL_RX, 0, 0);
    if (status) {
        ei_printf("ERR: I2S Control status = %d\n", status);
        return false;
    }

    return true;
}

/**
 * @brief Create a header object
 * 
 * @param payload 
 * @return true 
 * @return false 
 */
static bool create_header(sensor_aq_payload_info *payload)
{
    int ret;
    EiDeviceInfo *dev = EiDeviceInfo::get_device();
    EiDeviceMemory *mem = dev->get_memory();

    sensor_aq_init_mbedtls_hs256_context(&ei_mic_signing_ctx, &ei_mic_hs_ctx, dev->get_sample_hmac_key().c_str());

    ret = sensor_aq_init(&ei_mic_ctx, payload, NULL, true);

    if (ret != AQ_OK) {
        ei_printf("sensor_aq_init failed (%d)\n", ret);
        return false;
    }

    // then we're gonna find the last byte that is not 0x00 in the CBOR buffer.
    // That should give us the whole header
    size_t end_of_header_ix = 0;
    for (size_t ix = ei_mic_ctx.cbor_buffer.len - 1; ix != 0; ix--) {
        if (((uint8_t *)ei_mic_ctx.cbor_buffer.ptr)[ix] != 0x0) {
            end_of_header_ix = ix;
            break;
        }
    }

    if (end_of_header_ix == 0) {
        ei_printf("Failed to find end of header\n");
        return false;
    }

    // Write to blockdevice
    ret = mem->write_sample_data((uint8_t*)ei_mic_ctx.cbor_buffer.ptr, 0, end_of_header_ix);
    if ((size_t)ret != end_of_header_ix) {
        ei_printf("Failed to write to header blockdevice (%d)\n", ret);
        return false;
    }

    headerOffset = end_of_header_ix;

    return true;
}

/**
 * @brief Get the audio data object
 * 
 * @param callback 
 */
void ei_mic_thread(void (*callback)(void *buffer, uint32_t n_bytes))
{
    EventBits_t event_bit;

    event_bit = xEventGroupWaitBits(mic_event, 
        MIC_EVENT_RX_DONE | MIC_EVENT_RX_OVERFLOW | MIC_EVENT_ERROR,    //  uxBitsToWaitFor 
        pdTRUE,                 //  xClearOnExit
        pdFALSE,                //  xWaitForAllBits
        portMAX_DELAY);
    
    if (event_bit & MIC_EVENT_RX_DONE) {
        callback(mic_rec[audio_current_rec_buf], (AUDIO_REC_SAMPLES * n_audio_channels));   // in bytes !
    }
    if (event_bit & MIC_EVENT_RX_OVERFLOW) {
        ei_printf("I2S RX Overflow\n");
    }

    if (event_bit & MIC_EVENT_ERROR) {
        ei_printf("I2S Error\n");
    }
    
}

/**
 *
 * @param buffer
 * @param hdrLength
 * @return
 */
static int insert_ref(char *buffer, int hdrLength)
{
    #define EXTRA_BYTES(a)  ((a & 0x3) ? 4 - (a & 0x3) : (a & 0x03))

    const char *ref = "Ref-BINARY-i16";
    int addLength = 0;
    int padding = EXTRA_BYTES(hdrLength);

    buffer[addLength++] = 0x60 + 14 + padding;
    for(unsigned int i = 0; i < strlen(ref); i++) {
        buffer[addLength++] = *(ref + i);
    }
    for(int i = 0; i < padding; i++) {
        buffer[addLength++] = ' ';
    }

    buffer[addLength++] = 0xFF;

    return addLength;
}

/**
 * @brief Convert to CBOR int16 format and write to buf
 */
static void write_value_to_cbor_buffer(uint8_t *buf, int16_t value)
{
    uint8_t datatype;
    uint16_t sample;

    if(value < 0) {
        datatype = 0x39;
        /* Convert from 2's complement */
        sample = (uint16_t)~value + 1;
    }
    else {
        datatype = 0x19;
        sample = value;
    }
    buf[0] = datatype;
    buf[1] = sample >> 8;
    buf[2] = sample & 0xFF;    
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
static bool ei_microphone_init_channels(void)
{
    int32_t status = 0;
    constexpr uint32_t wlen = 32;

    /* Configure I2S Receiver to Asynchronous Master */
    status = s_i2s_drv->Control(ARM_SAI_CONFIGURE_RX | ARM_SAI_MODE_MASTER | ARM_SAI_ASYNCHRONOUS |
                                ARM_SAI_PROTOCOL_I2S | ARM_SAI_DATA_SIZE(wlen) | ((n_audio_channels == 1) << 19) ,
                                (wlen * 2),
                               (SAMPLING_RATE*n_audio_channels));

    if (status) {
        ei_printf("I2S Control status = %d\n", status);
        s_i2s_drv->PowerControl(ARM_POWER_OFF);
        s_i2s_drv->Uninitialize();
        return false;
    }

    status = s_i2s_drv->Control(ARM_SAI_CONTROL_RX, 1, 0); // Added here to keep recording going

    return (status == 0);
}

/**
 * 
 */
static uint32_t ei_mic_get_int16_from_buffer(int32_t* src, int16_t* dst, uint32_t element)
{
    // mic_buffer
    for (uint32_t i = 0; i < element; i++) {
        dst[i] = (int16_t)(src[i] >> 16);    /* shift */
    }

    return 0;
}
