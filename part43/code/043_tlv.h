#ifndef UNIX_OS_043_TLV_H
#define UNIX_OS_043_TLV_H

#include <stdint.h>

/* A real BER-TLV (Basic Encoding Rules, Tag-Length-Value) encoder/
 * decoder -- the real, general framing every EMV chip card data object
 * this book has ever seen actually rides in, from the card's own
 * response to SELECT down to the tags a GENERATE AC command returns.
 *
 * BER-TLV itself is defined by ISO/IEC 8825 (ASN.1 BER), a paid
 * standard this book has not read, the same honesty note as ISO 8583/
 * ISO 9564-1 since Chapters 33/38. Every rule below is instead cited
 * from a real, open-source, independently-written implementation read
 * in full: github.com/moov-io/bertlv (Go), its own tlv.go, cloned
 * locally this chapter. moov-io's own repository was already this
 * book's strongest ISO 8583 citation (Chapters 33/38); this chapter
 * reads a second one of their own repositories, this time for its own
 * general TLV framing rather than any one payment message format.
 *
 * The real tag-encoding rule (moov-io's own isMultiByte()/decodeTag()):
 * a tag's first byte's own low 5 bits (0x1F) all set means the tag
 * continues into further bytes; each continuation byte's own top bit
 * (0x80) set means "more bytes follow", and the last tag byte always
 * has that top bit clear. This chapter's own decoder supports tags up
 * to 2 real bytes (every EMV tag this chapter uses -- 82, 84, 94, 95,
 * 9A, 9C, 9F02, 9F1A, 9F10, 9F26, 9F27, 9F36, 9F37 -- fits in 1 or 2),
 * refusing outright rather than guessing at a longer one.
 *
 * The real length-encoding rule (moov-io's own encodeLength()/
 * decodeLength()): a length byte under 128 IS the length (short form);
 * a length byte with its own top bit set instead names how many
 * further big-endian length bytes follow (long form) -- BER's own
 * "indefinite length" (a length byte of exactly 0x80 with no following
 * bytes) is real but explicitly NOT supported here, the same refusal
 * moov-io's own decodeLength() makes, since no EMV data object this
 * chapter handles ever needs it.
 *
 * Stated limits of this implementation: at most TLV_MAX_OBJECTS real
 * top-level objects per buffer (this chapter never needs nested/
 * "constructed" tags -- EMV's own real 0x77/0x80 GENERATE AC response
 * templates are both flat lists of primitive tags in the shape this
 * chapter builds, so nested TLV decoding is out of scope, stated
 * plainly, not because BER-TLV itself lacks it); a tag is at most 2
 * real bytes; a length is at most 2 real bytes (values up to 65535),
 * comfortably above any EMV tag this chapter's own code ever builds. */

#define TLV_MAX_TAG_LEN 2u
#define TLV_MAX_VALUE_LEN 252u /* real EMV field widths never exceed this */
#define TLV_MAX_OBJECTS 16u

typedef struct {
    uint8_t tag[TLV_MAX_TAG_LEN];
    uint8_t tag_len;
    uint8_t value[TLV_MAX_VALUE_LEN];
    uint8_t value_len;
} tlv_object_t;

typedef struct {
    tlv_object_t objects[TLV_MAX_OBJECTS];
    uint32_t count;
} tlv_list_t;

/* Encodes every object in `list`, in order, into `out`. Returns the
 * encoded length, or 0 if `out_size` is too small or any object's own
 * tag/value length is out of range. */
uint32_t tlv_build(const tlv_list_t *list, uint8_t *out, uint32_t out_size);

/* Decodes exactly `len` bytes of `buf` into `out`. Returns 1 on
 * success, or 0 -- refusing outright -- on a real BER indefinite
 * length, a tag longer than TLV_MAX_TAG_LEN, a value longer than
 * TLV_MAX_VALUE_LEN, more than TLV_MAX_OBJECTS top-level objects, a
 * truncated tag/length/value, or any trailing byte that is not a real
 * '00' padding byte (moov-io's own decodeTag() comment: "Before,
 * between, or after TLV-coded data objects, '00' bytes without any
 * meaning may occur ... Ignore them"). */
int tlv_parse(const uint8_t *buf, uint32_t len, tlv_list_t *out);

/* Returns a pointer to the first object in `list` whose tag matches
 * `tag`/`tag_len` exactly, or 0 if none does. */
const tlv_object_t *tlv_find(const tlv_list_t *list, const uint8_t *tag, uint8_t tag_len);

#endif
