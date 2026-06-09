#ifndef __SPEAKER_HELPERS_H__
#define __SPEAKER_HELPERS_H__

#include "rpi.h"
#include "rpi-math.h"
#include "i2s.h"
#include "pi-random.h"
#include "shared.h"
#include "boot-crc32.h"

enum {
    gain_pin = 23,
    sd_pin = 25,
};

static void amplifier_enable(void) {
    gpio_set_output(gain_pin);
    gpio_set_output(sd_pin);
    put32(GPIO_CLR0, 1u << gain_pin);
    put32(GPIO_SET0, 1u << sd_pin);
}

static int64_t sine_table[TABLE_SIZE];
static int ready = 0;
static uint32_t phase_steps[NUM_FREQS];
static uint32_t sine_phase = 0;
static uint32_t start_time = 0;
static uint32_t period_count = 0;
static uint32_t phases[NUM_FREQS];

static inline void bits_to_str(const uint8_t bits[NUM_FREQS], char out[NUM_FREQS + 1]) {
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        out[i] = bits[i] ? '1' : '0';
    }
    out[NUM_FREQS] = '\0';
}

static inline void setup(){
    if (!ready) {
        for (unsigned i = 0; i < TABLE_SIZE; i++) {
            double theta = (2.0 * M_PI * (double)i) / (double)TABLE_SIZE;
            sine_table[i] = (int64_t)(sin(theta) * (double)0x7FFFFFFF);
        }
        ready = 1;
    }
    for (uint32_t i = 0; i < NUM_FREQS; i++) {
        phase_steps[i] = (uint32_t)(get_tone(i) / (double)SAMPLE_RATE * (double)(1ULL << 32));
        phases[i] = 0;
    }
}

static inline void play_combined(uint8_t* bits) {
    double active = 1.0;
    if (AMPLITUDE_SCALE) {
        active = 0.0;
        for (uint32_t i = 0; i < NUM_FREQS; i++) {
            active += bits[i]; 
        }
        if (active > 0) {
            active = sqrt(active);
        }
    }
    int64_t scaled_amplitude = AMPLITUDE / active;
    for (uint32_t i = 0; i < SPEAKER_FRAMES_PER_SYMBOL; i++) {
        int64_t sample = 0;
        for (uint32_t j = 0; j < NUM_FREQS; j++) {
            if (bits[j]) {
                uint32_t idx = phases[j] >> (32 - TABLE_BITS);
                sample += sine_table[idx];
            }
            phases[j] += phase_steps[j];
        }
        sample = (sample * scaled_amplitude) >> 31;
        i2s_put_frame(sample, sample);
    }
}

static void play_random_start() {
    uint32_t phase_step = (uint32_t)((double)get_tone(0) / (double)SAMPLE_RATE * (double)(1ULL << 32));

    for (uint32_t i = 0; i < SPEAKER_FRAMES_PER_SYMBOL; i++) {
        uint32_t idx = sine_phase >> (32 - TABLE_BITS);
        int64_t sample = ((int64_t)sine_table[idx] * pi_random()) >> 31;
        i2s_put_frame(sample, sample);
        sine_phase += phase_step;
    }
}

static inline void initial_synchronization(int end) {
    uint8_t bits[NUM_FREQS];
    uint8_t pattern[NUM_BYTES_PER_PERIOD];
    play_random_start();
 
    for (uint32_t i = 0; i < NUM_BYTES_PER_PERIOD; i++) {
        pattern[i] = SYNC_CALIBRATION_BYTE;
    }
    bytes_to_bits(pattern, bits);
    play_combined(bits);
 
    for (uint32_t i = 0; i < NUM_BYTES_PER_PERIOD; i++) {
        pattern[i] = end ? SYNC_END_BYTE : SYNC_MAGIC_BYTE ;
    }
    bytes_to_bits(pattern, bits);
    play_combined(bits);
}

static inline void send_header(uint16_t length, uint32_t checksum) {
    uint8_t bits[NUM_FREQS];
    uint8_t length_bytes[NUM_BYTES_PER_PERIOD];
    length_bytes[1] = (uint8_t)((length >> 8) & 0xFF);
    length_bytes[0] = (uint8_t)(length & 0xFF);
    bytes_to_bits(length_bytes, bits);
    play_combined(bits);
    uint8_t checksum_bytes[NUM_BYTES_PER_PERIOD];
    checksum_bytes[0] = (uint8_t)(checksum & 0xFF);
    checksum_bytes[1] = (uint8_t)((checksum >> 8) & 0xFF);
    bytes_to_bits(checksum_bytes, bits);
    play_combined(bits);
    checksum_bytes[0] = (uint8_t)((checksum >> 16) & 0xFF);
    checksum_bytes[1] = (uint8_t)((checksum >> 24) & 0xFF);
    bytes_to_bits(checksum_bytes, bits);
    play_combined(bits);
}

static inline void send_chunk(uint32_t length, payload_t* payload, int end) {
    payload->size = length;
    payload->cksum = crc32(payload->data, payload->size);
    uint8_t bits[NUM_FREQS];
    initial_synchronization(end);
    send_header(payload->size, payload->cksum);
    for (uint32_t i = 0; i < payload->size; i += NUM_BYTES_PER_PERIOD) {
        bytes_to_bits(&payload->data[i], bits);
        play_combined(bits);
    }
    delay_us(CHUNK_DELAY_US);
}

static inline void send_data(const uint8_t *data, uint32_t length) {
    payload_t payload;
    uint32_t num_chunks = length / PAYLOAD_MAX_BYTES;
    uint32_t last_chunk_length = length % (PAYLOAD_MAX_BYTES);
    for (uint32_t i = 0; i < num_chunks; i++) {
        memcpy(payload.data, &data[i * PAYLOAD_MAX_BYTES], PAYLOAD_MAX_BYTES);
        send_chunk(PAYLOAD_MAX_BYTES, &payload, 0);
    }
    if (last_chunk_length > 0) {
        memcpy(payload.data, &data[num_chunks * PAYLOAD_MAX_BYTES], last_chunk_length);
        send_chunk(last_chunk_length, &payload, 0);
    }
    send_chunk(0, &payload, 1);
    printk("sent %d bytes in %d chunk(s)\n", length, num_chunks + (last_chunk_length > 0));
}


static inline void play_sine(double freq) {
    uint32_t phase_step = (uint32_t)(freq / (double)SAMPLE_RATE * (double)(1ULL << 32));

    for (uint32_t i = 0; i < SPEAKER_FRAMES_PER_SYMBOL; i++) {
        uint32_t idx = sine_phase >> (32 - TABLE_BITS);
        int64_t sample = ((int64_t)sine_table[idx] * MAX_AMPLITUDE) >> 31;
        i2s_put_frame(sample, sample);
        sine_phase += phase_step;
    }
}

#endif