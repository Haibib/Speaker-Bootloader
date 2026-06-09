#ifndef __MIC_HELPERS_H__
#define __MIC_HELPERS_H__

#include "rpi.h"
#include "rpi-math.h"
#include "i2s.h"
#include "shared.h"
#include "boot-crc32.h"

static double symbol_samples[SYMBOL_SAMPLES]; 
static double window_samples[WINDOW_SAMPLES]; 
static double tone_threshold[NUM_FREQS];

// static double org_goertzel(const double *samples, int num_samples, double target_tone) {
//     double target_bin = target_tone / (double)SAMPLE_RATE * num_samples;
//     double width = 2.0 * M_PI * target_bin / num_samples;
//     double cos_width = cos(width);
//     double coefficient = 2.0 * cos_width;
 
//     double first_sample = 0, second_sample = 0;
//     for (int i = 0; i < num_samples; i++) {
//         double current_sample = samples[i] + coefficient * first_sample - second_sample;
//         second_sample = first_sample;
//         first_sample = current_sample;
//     }
//     double real_component = first_sample - second_sample * cos_width;
//     double imaginary_component = second_sample * sin(width);
//     return imaginary_component * imaginary_component + real_component * real_component;
// }

static double goertzel(const double *samples, int num_samples, double target_tone) {
    double k = (double)num_samples * target_tone / (double)SAMPLE_RATE;
    double coefficient = 2.0 * cos(2.0 * M_PI * k / (double)num_samples);
 
    double first_sample = 0, second_sample = 0;
    for (int i = 0; i < num_samples; i++) {
        double current_sample = samples[i] + coefficient * first_sample - second_sample;
        second_sample = first_sample;
        first_sample = current_sample;
    }
    return first_sample * first_sample + second_sample * second_sample - coefficient * first_sample * second_sample;
}

static void read_symbol(void) {
    for (int i = 0; i < SYMBOL_SAMPLES; i++) {
        symbol_samples[i] = (int32_t)i2s_get32() / 2147483648.0;
    }
    for (int i = 0; i < WINDOW_SAMPLES; i++) {
        window_samples[i] = symbol_samples[WINDOW_OFFSET + i];
    }
    double mean = 0;
    for (int i = 0; i < WINDOW_SAMPLES; i++) {
        mean += window_samples[i];
    }
    mean /= WINDOW_SAMPLES;
    for (int i = 0; i < WINDOW_SAMPLES; i++) {
        window_samples[i] -= mean;
    }
}

static void get_magnitudes(double magnitudes[NUM_FREQS]) {
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        magnitudes[i] = sqrt(goertzel(window_samples, WINDOW_SAMPLES, get_tone(i)));
    }
}

static double signal_variance(int num_samples) {
    double mean = 0;
    for (int i = 0; i < num_samples; i++) {
        symbol_samples[i] = (int32_t)i2s_get32() / 2147483648.0;
        mean += symbol_samples[i];
    }
    mean /= num_samples;
 
    double variance = 0;
    for (int i = 0; i < num_samples; i++) {
        double d = symbol_samples[i] - mean;
        variance += d * d;
    }
    return variance / num_samples;
}


static void calibrate_thresholds(double *magnitude_outputs) {
    double magnitudes[NUM_CALIBRATION_SYMBOLS][NUM_FREQS];
    for(int i=0 ; i<NUM_CALIBRATION_SYMBOLS; i++){
        read_symbol();
        get_magnitudes(&magnitudes[i][0]);
    }
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        double sum = 0;
        for(int j=0; j<NUM_CALIBRATION_SYMBOLS; j++) {
            sum += magnitudes[j][i];
        }
        tone_threshold[i] = THRESHHOLD_SCALE * (sum / (double)NUM_CALIBRATION_SYMBOLS);
        if (magnitude_outputs) {
            magnitude_outputs[i] = tone_threshold[i] / THRESHHOLD_SCALE;
        }
    }
}

static uint32_t detect_mask(double *magnitude_outputs) {
    double magnitudes[NUM_FREQS];
    read_symbol();
    get_magnitudes(magnitudes);
 
    uint32_t mask = 0;
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        if (magnitude_outputs) {
            magnitude_outputs[i] = magnitudes[i];
        }
        if (magnitudes[i] > tone_threshold[i]) {
            mask |= (1u << i);
        }
    }
    return mask;
}

static inline uint32_t get_mask(uint8_t byte) {
    uint32_t mask = 0;
    for (uint32_t i = 0; i < NUM_BYTES_PER_PERIOD; i++) {
        mask |= (uint32_t)byte << (i * 8);
    }
    return mask;
}

static inline void get_bytes(uint32_t mask, uint8_t bytes[NUM_BYTES_PER_PERIOD]) {
    for (uint32_t i = 0; i < NUM_BYTES_PER_PERIOD; i++) {
        bytes[i] = (mask >> (i * 8)) & 0xFF;
    }
}

static void print_debug(double variance, const double *calibration_magnitudes, const double *magic_magnitudes, uint32_t magic_mask, uint32_t expected_mask) {
    printk("variance: got %f, gate %f\n", variance, VARIANCE_GATE);
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        int hit = magic_magnitudes[i] > tone_threshold[i];
        printk("tone %d  %d Hz  cal=%f  thresh=%f  magic=%f  %s\n", i, (int)get_tone(i), calibration_magnitudes[i], tone_threshold[i], magic_magnitudes[i], hit ? "HIT" : "---");
    }
    uint8_t got_bytes[NUM_BYTES_PER_PERIOD];
    uint8_t want_bytes[NUM_BYTES_PER_PERIOD];
    get_bytes(magic_mask, got_bytes);
    get_bytes(expected_mask, want_bytes);
    if (magic_mask != expected_mask) {
        printk("got magic: mask=%x byte0=%x | expected: mask=%x byte0=%x\n", magic_mask, got_bytes[0], expected_mask, want_bytes[0]);
    }
}

enum {
    CHUNK_START = 1,
    CHUNK_END = 2,
    FALSE_START = 3,
    ERROR = 4,
};

static int wait_for_sync(uint32_t verbose) {
    while (signal_variance(NUM_SAMPLES_VARIANCE) < VARIANCE_GATE);
    read_symbol();
    calibrate_thresholds(NULL);
    uint32_t magic_mask = detect_mask(NULL);
    if(magic_mask == get_mask(SYNC_MAGIC_BYTE)) {
        return CHUNK_START;
    } else if (magic_mask == get_mask(SYNC_END_BYTE)) {
        return CHUNK_END;
    }
    return FALSE_START;
}

static int receive_chunk(payload_t *payload) {
    int sync = wait_for_sync(0);
    if (sync == FALSE_START) {
        return FALSE_START;
    }
    uint16_t length = detect_mask(NULL) & 0xFFFF;
    uint32_t checksum_low_bytes = detect_mask(NULL);
    uint32_t checksum_high_bytes = detect_mask(NULL);
    uint32_t received_checksum = (checksum_high_bytes << 16) | checksum_low_bytes;

    if (length > PAYLOAD_MAX_BYTES || length % NUM_BYTES_PER_PERIOD != 0) {
        printk("bad header: length=%d\n", length);
        return ERROR;
    }
    uint32_t num_periods = length / NUM_BYTES_PER_PERIOD;
    for (uint32_t i = 0; i < num_periods; i++) {
        uint32_t mask = detect_mask(NULL);
        payload->data[i * NUM_BYTES_PER_PERIOD] = mask & 0xFF;
        payload->data[i * NUM_BYTES_PER_PERIOD + 1] = (mask >> 8) & 0xFF;
    }
    payload->size = length;
    payload->cksum = received_checksum;
    uint32_t computed_cksum = crc32(payload->data, length);
    if (computed_cksum != received_checksum) {
        printk("checksum mismatch: received=%x computed=%x size=%d\n", received_checksum, computed_cksum, length);
        return ERROR;
    }
    return sync;
}

static uint32_t receive_data(uint8_t *destination, int verbose) {
    uint32_t total_bytes = 0;
    payload_t payload;
 
    while (1) {
        int result = receive_chunk(&payload);
        if (result == FALSE_START) {
            continue;
        } else if (result == ERROR) {
            rpi_reboot();
        }
        memcpy(&destination[total_bytes], payload.data, payload.size);
        total_bytes += payload.size;
        if (result == CHUNK_END) {
            break;
        }
    }
    // for(int i=0;i<NUM_FREQS;i++) {
    //     printk("tone %d  %d Hz  thresh=%f\n", i, (int)get_tone(i), tone_threshold[i]);
    // }
 
    return total_bytes;
}

#endif