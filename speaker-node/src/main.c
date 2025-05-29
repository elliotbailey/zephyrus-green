/**
******************************************************************************
* @file    project/main.c
* @author  Travis Graham - 473437553
* @date    29/05/25
* @brief   Project Zephyrus Green
******************************************************************************
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/dac.h>
#include <math.h>

#include "../inc/mqtt.h"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif


LOG_MODULE_REGISTER(dac_audio, LOG_LEVEL_INF);

// Get DAC device from device tree
const struct device *dac_dev = DEVICE_DT_GET(DT_NODELABEL(dac));

// Audio parameters
#define SAMPLE_RATE_HZ      8000    // 8kHz sample rate
#define FREQUENCY_HZ        400     // A4 note (440Hz)
#define AMPLITUDE           50      // Max amplitude (0-255 range, centered at 128)
#define DURATION_SECONDS    5       // Play for 3 seconds

int amplitude = 255;

// Calculate samples needed
#define TOTAL_SAMPLES       (SAMPLE_RATE_HZ * DURATION_SECONDS)
#define SAMPLES_PER_CYCLE   (SAMPLE_RATE_HZ / FREQUENCY_HZ)

// Timer for audio playback
static struct k_timer audio_timer;
static uint32_t sample_index = 0;
static bool playing = false;

/**
* Initialise the DAC on GPIO 25 on the ESP32
*/
static int init_dac(void) {
    if (!device_is_ready(dac_dev)) {
        LOG_ERR("DAC device not ready");
        return -1;
    }
    
    // Configure DAC channel 0 (GPIO25)
    struct dac_channel_cfg dac_ch_cfg = {
        .channel_id = 0,        // GPIO25
        .resolution = 8,        // 8-bit resolution
        .buffered = false       // Direct output
    };
    
    int ret = dac_channel_setup(dac_dev, &dac_ch_cfg);
    if (ret < 0) {
        LOG_ERR("Failed to setup DAC channel: %d", ret);
        return ret;
    }
    
    LOG_INF("DAC initialized successfully on GPIO25");
    return 0;
}

/**
 * Given a sample number and a frequency, generate a sin wave represeneting the 
 * chosen frequency and return the representative DAC value
*/
static uint8_t generate_tone_sample(uint32_t sample_num, uint16_t frequency) {
    double phase = (2.0 * M_PI * frequency * sample_num) / SAMPLE_RATE_HZ;
    double sine_value = sin(phase);
    uint8_t dac_value = (uint8_t)(128 + (amplitude * sine_value));
    return dac_value;
}

/**
 * Given two tones and a duration for each, play the tone on the DAC and over 
 * the speaker.
*/
static void play_two_tone_sequence(uint32_t duration1, uint32_t duration2, uint16_t freq1, uint16_t freq2) {
    uint32_t total_samples = duration1 + duration2;

    for (uint32_t sample_index = 0; sample_index < total_samples; sample_index++) {
        uint8_t sample;

        if (sample_index < duration1) {
            // First tone
            sample = generate_tone_sample(sample_index, freq1);
        } else {
            uint32_t tone2_sample = sample_index - duration1;
            // Second tone
            sample = generate_tone_sample(tone2_sample, freq2);
        }
        // Output sample to DAC
        dac_write_value(dac_dev, 0, sample);
    }

    printf("Two-tone playback completed\r\n");
}


/**
 * Given a single tones and a duration play the tone on the DAC and over 
 * the speaker.
*/
static void play_single_tone(uint32_t duration, uint16_t freq) {
    for (uint32_t sample_index = 0; sample_index < duration; sample_index++) {
        uint8_t sample;
        // Generate single tone
        sample = generate_tone_sample(sample_index, freq);
        // Output sample to DAC
        dac_write_value(dac_dev, 0, sample);
    }
}

int extract_volume(const char *str, int *value) {
    return sscanf(str, "Volume %d", value) == 1;
}

int main(void) {
    LOG_INF("ESP32 DAC Audio Player Starting...");
    
    // Initialize DAC
    if (init_dac() < 0) {
        LOG_ERR("Failed to initialize DAC");
        return -1;
    }

    play_two_tone_sequence(10000, 10000, 800, 400);  // B sequence: 400Hz->800Hz

    // Main loop - could add user controls here
    while (1) {
        struct mqtt_t rx_mqtt;

        k_msgq_get(&mqtt_msgq, &rx_mqtt, K_FOREVER);

        printf("Received from queue %s\r\n", rx_mqtt.rx_buff);

        if (extract_volume(rx_mqtt.rx_buff, &amplitude)) {
          printf("Volume set to %d\r\n", amplitude);
          play_single_tone(10000, 800);
        } else if (strcmp(rx_mqtt.rx_buff, "Arrive") == 0) {
          play_two_tone_sequence(10000, 10000, 800, 400);
        } else if (strcmp(rx_mqtt.rx_buff, "Depart") == 0) {
          play_two_tone_sequence(10000, 10000, 400, 800);
        }
    }

    return 0;
}
