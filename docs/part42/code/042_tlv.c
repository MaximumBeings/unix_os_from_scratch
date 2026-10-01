/* See 042_tlv.h's own top-of-file comment for the citation of every
 * BER-TLV rule used here. */

#include "042_tlv.h"

/* moov-io's own isMultiByte(): bits 1-5 (mask 0x1F) of the first tag
 * byte all set means the tag continues into further bytes. */
static int is_multi_byte(uint8_t first) {
    return (first & 0x1Fu) == 0x1Fu;
}

uint32_t tlv_build(const tlv_list_t *list, uint8_t *out, uint32_t out_size) {
    uint32_t pos = 0;
    for (uint32_t i = 0; i < list->count; i++) {
        const tlv_object_t *o = &list->objects[i];
        if (o->tag_len == 0u || o->tag_len > TLV_MAX_TAG_LEN ||
            o->value_len > TLV_MAX_VALUE_LEN) {
            return 0;
        }
        /* moov-io's own encodeLength(): short form under 128, else one
         * length-count byte (0x80 | count) followed by `count`
         * big-endian bytes -- this chapter's own values never need
         * more than one, since TLV_MAX_VALUE_LEN fits a single byte. */
        uint32_t len_bytes = (o->value_len < 128u) ? 1u : 2u;
        if (pos + o->tag_len + len_bytes + o->value_len > out_size) {
            return 0;
        }
        for (uint32_t i2 = 0; i2 < o->tag_len; i2++) {
            out[pos + i2] = o->tag[i2];
        }
        pos += o->tag_len;
        if (len_bytes == 1u) {
            out[pos] = o->value_len;
        } else {
            out[pos] = 0x81u;
            out[pos + 1u] = o->value_len;
        }
        pos += len_bytes;
        for (uint32_t i2 = 0; i2 < o->value_len; i2++) {
            out[pos + i2] = o->value[i2];
        }
        pos += o->value_len;
    }
    return pos;
}

int tlv_parse(const uint8_t *buf, uint32_t len, tlv_list_t *out) {
    out->count = 0;
    uint32_t pos = 0;
    while (pos < len) {
        /* "'00' bytes without any meaning may occur ... Ignore them." */
        if (buf[pos] == 0x00u) {
            pos++;
            continue;
        }
        if (out->count >= TLV_MAX_OBJECTS) {
            return 0;
        }
        tlv_object_t *o = &out->objects[out->count];
        uint32_t tag_len = 1;
        if (is_multi_byte(buf[pos])) {
            tag_len = 2; /* TLV_MAX_TAG_LEN's own stated limit */
            if (pos + 1u >= len) {
                return 0; /* truncated tag */
            }
            if ((buf[pos + 1u] & 0x80u) != 0u) {
                return 0; /* a third tag byte would exceed our own limit */
            }
        }
        if (pos + tag_len > len) {
            return 0;
        }
        for (uint32_t i = 0; i < tag_len; i++) {
            o->tag[i] = buf[pos + i];
        }
        o->tag_len = (uint8_t) tag_len;
        pos += tag_len;

        if (pos >= len) {
            return 0; /* truncated length */
        }
        uint32_t value_len;
        if ((buf[pos] & 0x80u) == 0u) {
            value_len = buf[pos];
            pos += 1u;
        } else {
            uint32_t count = buf[pos] & 0x7Fu;
            if (count == 0u) {
                return 0; /* real BER indefinite length -- not supported */
            }
            if (count > 1u) {
                return 0; /* stated limit: at most one length-count byte */
            }
            if (pos + 1u + count > len) {
                return 0; /* truncated length */
            }
            value_len = buf[pos + 1u];
            pos += 1u + count;
        }
        if (value_len > TLV_MAX_VALUE_LEN || pos + value_len > len) {
            return 0;
        }
        for (uint32_t i = 0; i < value_len; i++) {
            o->value[i] = buf[pos + i];
        }
        o->value_len = (uint8_t) value_len;
        pos += value_len;
        out->count++;
    }
    return 1;
}

const tlv_object_t *tlv_find(const tlv_list_t *list, const uint8_t *tag, uint8_t tag_len) {
    for (uint32_t i = 0; i < list->count; i++) {
        const tlv_object_t *o = &list->objects[i];
        if (o->tag_len != tag_len) {
            continue;
        }
        int match = 1;
        for (uint32_t j = 0; j < tag_len; j++) {
            if (o->tag[j] != tag[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            return o;
        }
    }
    return 0;
}
