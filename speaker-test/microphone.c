#include "rpi.h"
#include "i2s.h"
#include "math.h"


typedef struct { double r, i; } complex;
static complex cx_add(complex a, complex b) { return (complex){a.r+b.r, a.i+b.i}; }
static complex cx_sub(complex a, complex b) { return (complex){a.r-b.r, a.i-b.i}; }
static complex cx_mul(complex a, complex b) { return (complex){a.r*b.r - a.i*b.i, a.r*b.i + a.i*b.r}; }
static double cx_mag2(complex a)  { return a.r*a.r + a.i*a.i; }

static void bit_reverse(complex *x, int n) {
    int bits = 0;
    while ((1 << bits) < n) bits++;
    for (int i = 0; i < n; i++) {
        int rev = 0;
        for (int j = 0; j < bits; j++)
            if (i & (1 << j)) rev |= 1 << (bits - j - 1);
        if (i < rev) { complex t = x[i]; x[i] = x[rev]; x[rev] = t; }
    }
}

static void fft(complex *x, int n) {
    bit_reverse(x, n);
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        complex w = { .r = cos(ang), .i = sin(ang) };
        for (int i = 0; i < n; i += len) {
            complex wn = {1.0, 0.0};
            for (int j = 0; j < len/2; j++) {
                complex u = x[i+j];
                complex v = cx_mul(x[i+j+len/2], wn);
                x[i+j]        = cx_add(u, v);
                x[i+j+len/2]  = cx_sub(u, v);
                wn = cx_mul(wn, w);
            }
        }
    }
}

static void apply_hann(double *s, int n) {
    for (int i = 0; i < n; i++)
        s[i] *= 0.5 * (1.0 - cos(2.0 * M_PI * i / (n - 1)));
}


enum {
    SAMPLE_RATE = 60000,
    N           = 512,  
};

#define FFT_N 1024
#define FFT_MIDDLE 512
#define FFT_OFFSET ((FFT_N - FFT_MIDDLE) / 2)  // 256

static complex fft_tmp[FFT_N];
static double  samples_tmp[FFT_N];


int get_top_freq(void) {
    int dc_skip = (int)(20.0 / ((double)SAMPLE_RATE / FFT_MIDDLE)) + 1;

    for (int i = 0; i < FFT_N; i++)
        samples_tmp[i] = (int32_t)i2s_get32() / 2147483648.0;

    double mean = 0;
    for (int i = 0; i < FFT_N; i++) mean += samples_tmp[i];
    mean /= FFT_N;
    for (int i = 0; i < FFT_N; i++) samples_tmp[i] -= mean;

    double middle[FFT_MIDDLE];
    for (int i = 0; i < FFT_MIDDLE; i++)
        middle[i] = samples_tmp[FFT_OFFSET + i];
    apply_hann(middle, FFT_MIDDLE);


    for (int i = 0; i < FFT_MIDDLE; i++)
        fft_tmp[i] = (complex){ middle[i], 0.0 };
    fft(fft_tmp, FFT_MIDDLE);


    double bin_hz = (double)SAMPLE_RATE / FFT_MIDDLE;
    double peak_mag2 = 0;
    int    peak_bin  = dc_skip;
    for (int i = dc_skip; i < FFT_MIDDLE / 2; i++) {
        double m2 = cx_mag2(fft_tmp[i]);
        if (m2 > peak_mag2) { peak_mag2 = m2; peak_bin = i; }
    }

    return (int)(peak_bin * bin_hz);
}

double compute_start_variance(int start_n) {
    for (int i = 0; i < start_n; i++)
        samples_tmp[i] = (int32_t)i2s_get32() / 2147483648.0;
    
    double mean = 0;
    for (int i = 0; i < start_n; i++)
        mean += samples_tmp[i];
    mean /= start_n;

    double var = 0;
    for (int i = 0; i < start_n; i++) {
        double diff = samples_tmp[i] - mean;
        var += diff * diff;
    }
    return var / start_n;
}

void notmain(void) {
    caches_enable();
    i2s_init(SAMPLE_RATE);
    i2s_rx_enable();


    int cnt = 0, cnt2=0;
    while(compute_start_variance(10)<0.01) {}

    int buffer[1024];
    for(int i=0;i<100;i++) {
        uint32_t start = timer_get_usec();
        int f = get_top_freq();
        uint32_t end = timer_get_usec();
        if(f > 0) {
            buffer[cnt++] = f;
        }
        output("Time taken: %d us\n", end - start);
    }

    for(int i=0;i<cnt;i++) {
        output("freq[%d]: %d\n", i, buffer[i]);
    }

}