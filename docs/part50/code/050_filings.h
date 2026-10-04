/* Chapter 48: the six embedded filings and the helpers the demo uses to print them. */
#ifndef FILINGS_H
#define FILINGS_H
#include <stdint.h>
#include "050_ratios.h"
typedef struct { const char *ticker, *company, *form, *filed, *accession, *url; const char *start, *end; } filing_t;
#define N_FILINGS 6
extern const filing_t g_filings[N_FILINGS];
/* "12.34%" / "1.50x" / "$99,584,000,000" / "$6.1300" for one value line; returns the length written (buf must hold 40 bytes) */
int edgar_format(const rt_line_t *l, char *buf);
/* the date a day-number stands for, "YYYY-MM-DD" (11 bytes) */
void edgar_date(int32_t days, char *buf);
/* first occurrence of needle in hay[0..n), or -1 */
int32_t edgar_find(const char *hay, uint32_t n, const char *needle);
#endif
