/* Chapter 49: the embedded sample files (see 050_samples_data.asm). */
#ifndef SAMPLES_H
#define SAMPLES_H
typedef struct { const char *name, *what; const char *start, *end; } sample_t;
#define N_REAL 10
#define N_CLAIMS 6
extern const sample_t g_real[N_REAL];
extern const sample_t g_claims[N_CLAIMS];
#endif
