/* Chapter 49: an X12 EDI reader. Healthcare claims (837) and remittances (835) travel as X12: a stream of SEGMENTS, each a short identifier (CLM, SV1, NM1, ...) followed by ELEMENTS, separated by a character the SENDER chooses
 * and announces in the very first segment. That first segment, ISA, is FIXED WIDTH (exactly 106 characters) and carries the three separators: the element separator (character 4, so "ISA*00*..."), the component separator (character 105)
 * and the segment terminator (character 106). Everything else is a variable-length list. A transmission nests like this:
 *     ISA interchange --- GS functional group --- ST transaction set (the claim, the remittance) --- SE --- GE --- IEA
 * and every closing segment repeats a control number and a COUNT of what it closed, which is how a receiver learns that part of a file went missing. This reader checks the envelope strictly and hands out segments by pointer into the
 * caller's text (no copy, no allocation). It does not know what a CLM or an SV1 means: that is 049_claim.h and 049_remit.h. */
#ifndef X12_H
#define X12_H
#include <stdint.h>

#define X12_MAX_SEG 4096

enum { X12_OK = 0, X12_ERR_NOT_X12 = -1, X12_ERR_SHORT_ISA = -2, X12_ERR_BAD_ISA = -3, X12_ERR_BAD_SEP = -4, X12_ERR_BAD_SEG = -5, X12_ERR_EMPTY_SEG = -6, X12_ERR_BAD_CHAR = -7, X12_ERR_TRUNCATED = -8,
       X12_ERR_TOO_MANY_SEG = -9, X12_ERR_NESTING = -10, X12_ERR_MULTI_ISA = -11, X12_ERR_COUNT_ST = -12, X12_ERR_CTL_ST = -13, X12_ERR_COUNT_GE = -14, X12_ERR_CTL_GS = -15, X12_ERR_COUNT_IEA = -16, X12_ERR_CTL_ISA = -17,
       X12_ERR_AFTER_IEA = -18, X12_ERR_NO_IEA = -19 };

typedef struct { const char *p; uint16_t len; uint8_t id_len, n_el; } x12_seg_t; /* p: the segment text without its terminator; n_el: how many elements follow the identifier */
typedef struct {
    char esep, csep, rsep, term; uint16_t n_seg, n_gs, n_st; uint16_t err_seg; /* index of the segment an error was found in */
    uint8_t lenient; /* set by the caller before x12_parse(): count and control-number mismatches (SE01, SE02, GE01, GE02, IEA01, IEA02) are then recorded in warn instead of refused */
    uint32_t warn;   /* bit k set: the error code -k was tolerated (see lenient) */
    x12_seg_t seg[X12_MAX_SEG];
} x12_t;

int x12_parse(x12_t *x, const char *text, uint32_t len); /* X12_OK or a negative X12_ERR_*; x->err_seg says where */
const char *x12_strerror(int rc);
int x12_is(const x12_seg_t *s, const char *id); /* identifier equals id */
const char *x12_el(const x12_seg_t *s, int i, uint16_t *len); /* element i (1-based); NULL when the segment has fewer elements; an empty element is a non-NULL pointer with *len == 0 */
const char *x12_comp(const x12_t *x, const char *el, uint16_t el_len, int i, uint16_t *len); /* component i (0-based) of a composite element; NULL when absent */
int x12_money(const char *s, uint16_t n, int64_t *cents); /* "-12.5" -> -1250: optional '-', digits, optionally '.' then one or two digits; nothing else; 0 on success */
int x12_uint(const char *s, uint16_t n, uint32_t *v); /* digits only, at most 9 */
int x12_date(const char *s, uint16_t n, int32_t *days); /* CCYYMMDD, a real calendar date; days since 1970-01-01; 0 on success */
int x12_str_eq(const char *s, uint16_t n, const char *lit);
void x12_money_str(int64_t cents, char *out); /* the shortest X12 form: 0 -> "0", 12700 -> "127", 12780 -> "127.8", 12705 -> "127.05"; out >= 24 bytes */
#endif
