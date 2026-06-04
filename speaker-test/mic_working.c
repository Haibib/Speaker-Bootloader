#include "rpi.h"
#include "i2s.h"
#include "math.h"

enum {
    SAMPLE_RATE  = 59000,
    SYM_SAMPLES  = 1024,
    SYM_MIDDLE   = 512,
    SYM_OFFSET   = (SYM_SAMPLES - SYM_MIDDLE) / 2,
    MAX_TONES    = 32,
};

static double samples_tmp[SYM_SAMPLES];
static double samples_mid[SYM_MIDDLE];

static double goertzel(const double *samples, int n,
                       double target_hz, double sample_rate) {
    double k     = target_hz / sample_rate * n;
    double w     = 2.0 * M_PI * k / n;
    double coeff = 2.0 * cos(w);

    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < n; i++) {
        s0 = samples[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    double real = s1 - s2 * cos(w);
    double imag = s2 * sin(w);
    return real*real + imag*imag;
}


uint32_t goertzel_detect(const double *tones,   int    n_tones,
                         double        noise_hz, double thresh_mul, int debug, double *mag, double *threshold_mag, int cal) {

    for (int i = 0; i < SYM_SAMPLES; i++)
        samples_tmp[i] = (int32_t)i2s_get32() / 2147483648.0;

    for (int i = 0; i < SYM_MIDDLE; i++)
        samples_mid[i] = samples_tmp[SYM_OFFSET + i];

    double mean = 0;
    for (int i = 0; i < SYM_MIDDLE; i++) mean += samples_mid[i];
    mean /= SYM_MIDDLE;
    for (int i = 0; i < SYM_MIDDLE; i++) samples_mid[i] -= mean;

    double mags[MAX_TONES];
    double max_mag = 0;
    double sum_mag = 0;
    for (int i = 0; i < n_tones && i < MAX_TONES; i++) {
        mags[i] = sqrt(goertzel(samples_mid, SYM_MIDDLE, tones[i], SAMPLE_RATE));
        if (mags[i] > max_mag) max_mag = mags[i];
        sum_mag += mags[i];
    }

    double rel_thresh = max_mag * 0.10;

    if(debug)
        output("noise=%f  thresh=%f  max_mag=%f  rel_thresh=%f\n",
               0, 0, max_mag, rel_thresh);

    uint32_t mask = 0;
    for (int i = 0; i < n_tones && i < MAX_TONES; i++) {
        // int hit = (mags[i] > thresh) && (mags[i] > rel_thresh);
        if(cal==2) {
            threshold_mag[i] = mags[i] ;
        } else if(cal==1) {
            threshold_mag[i] = (threshold_mag[i] + mags[i]) / 4.0;
            mag[i] = mags[i];
        } else {
            int hit = (mags[i] > threshold_mag[i]);
            if (debug) output("  %d Hz: mag=%f  %s\n", (int)tones[i], mags[i], hit ? "HIT" : "---");
            mag[i] = mags[i];
            if (hit) mask |= (1u << i);
        }
    }

    return mask;
}


double compute_start_variance(int start_n) {
    for (int i = 0; i < start_n; i++)
        samples_tmp[i] = (int32_t)i2s_get32() / 2147483648.0;

    double mean = 0;
    for (int i = 0; i < start_n; i++) mean += samples_tmp[i];
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

   
        // uint32_t start = timer_get_usec();
        // for (int i = 0; i < SYM_SAMPLES; i++)
        //     i2s_get32();
        // uint32_t end   = timer_get_usec();
        // output("time=%d\n", end - start);


    double tones[] = { 1000, 2000, 3000, 4000,
                       5000,  6000, 7000, 8000};
    int    n_tones = sizeof(tones) / sizeof(tones[0]);


    enum {num_bytes = 26};
    uint32_t buf[num_bytes];
    double mag[num_bytes][8];
    double threshold_mag[8];

    for(int i=0;i<8;i++) {
        threshold_mag[i] = 0;
    }


    for(int i=0;i<20;i++) {

    while (1) {
        while(compute_start_variance(10) < 0.001) {}
        // this is noise ignore
        goertzel_detect(tones, n_tones, 500.0, 4.0, 0, &mag[0][0], &threshold_mag[0], 0);
        
        // this should be 0xFF
        uint32_t mask1  = goertzel_detect(tones, n_tones, 500.0, 4.0, 0, &mag[0][0], &threshold_mag[0], 1);
        mask1  = goertzel_detect(tones, n_tones, 500.0, 4.0, 0, &mag[0][0], &threshold_mag[0], 0);
        // uint32_t mask2  = goertzel_detect(tones, n_tones, 500.0, 4.0, 0);

        if(mask1 != 0x0F) {
            output("Expected 0x0F but got %x\n", mask1);
            for(int j=0;j<n_tones;j++) {
                int hit = (mag[0][j] > threshold_mag[j]);
                output("  %d Hz: mag=%f, threshold=%f  %s\n", (int)tones[j], mag[0][j], threshold_mag[j], hit ? "HIT" : "---");
            }
            continue;
        }

        for(int i=0;i<num_bytes;i++) {
            buf[i] = goertzel_detect(tones, n_tones, 500.0, 4.0, 0, &mag[i][0], &threshold_mag[0], 0);
        }
        break;
    }
    int count = 0;
    for(int i=0;i<num_bytes;i++) {
        if(buf[i%26]!='A'+(i%26)) {
            count++;
            output("buf[%d]=%c, expected %c \n", i, buf[i], 'A'+(i%26));
            for(int j=0;j<8;j++) {
                int hit = (mag[i][j] > threshold_mag[j]);
                output("  %d Hz: mag=%f  %s\n", (int)tones[j], mag[i][j], hit ? "HIT" : "---");
            }
        }
    }
    output("count=%d\n", count);

    for(int i=0;i<num_bytes;i++){
        output("%c", buf[i]);
    }
    }

    // for (int i = 0; i < 100; i++) {
    //     uint32_t start = timer_get_usec();
    //     uint32_t mask  = goertzel_detect(tones, n_tones, 500.0, 4.0);
    //     uint32_t end   = timer_get_usec();
    //     output("mask=%x  time=%d\n", mask, end - start);
    // }
}