#ifndef __MIC_HELPERS_H__
#define __MIC_HELPERS_H__

#include "rpi.h"
#include "rpi-math.h"
#include "i2s.h"
#include "shared.h"

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
    double k = (int)(0.5 + ((double)num_samples * target_tone) / (double)SAMPLE_RATE);
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
    read_symbol();
    double magnitudes[NUM_FREQS];
    get_magnitudes(magnitudes);
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        tone_threshold[i] = THRESHHOLD_SCALE * magnitudes[i];
        if (magnitude_outputs) {
            magnitude_outputs[i] = magnitudes[i];
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

static int wait_for_sync(uint32_t verbose) {
    double variance = 0;
    while ((variance = signal_variance(10)) < VARIANCE_GATE) {}
    read_symbol();
    double calibration_magnitudes[NUM_FREQS];
    calibrate_thresholds(calibration_magnitudes);
    double magic_magnitudes[NUM_FREQS];
    uint32_t magic_mask = detect_mask(magic_magnitudes);
    uint32_t expected_mask = get_mask(SYNC_MAGIC_BYTE);
    if (verbose) {
        print_debug(variance, calibration_magnitudes, magic_magnitudes, magic_mask, expected_mask);
    }
    return magic_mask == expected_mask;
}

#endif