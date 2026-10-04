/* Chapter 48 host tool: parse one XBRL file with the kernel's own parser and print the engine's canonical output (the text ratios_ref.py prints). Usage: ratios_cli FILE [price_cents] */
#include <stdio.h>
#include <stdlib.h>
#include "../048_xbrl.h"
#include "../048_ratios.h"
static xb_doc_t doc; static rt_report_t rep;
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror("open"); return 2; }
    static char buf[1 << 22]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
    int rc = xb_parse(&doc, buf, (uint32_t)n); if (rc) { printf("PARSE_ERROR %d %s\n", rc, xb_strerror(rc)); return 1; }
    rc = rt_analyse(&doc, argc > 2 ? atoll(argv[2]) : 0, &rep); if (rc) { printf("ANALYSE_ERROR %d %s\n", rc, xb_strerror(rc)); return 1; }
    static char out[8192]; if (rt_canonical(&rep, out, sizeof out) < 0) { printf("BUFFER\n"); return 1; }
    fputs(out, stdout); return 0;
}
