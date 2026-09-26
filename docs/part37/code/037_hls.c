/* See 037_hls.h's own top-of-file comment for the full citation trail
 * (every real tag and the real fixture text, read directly out of a
 * clone of video-dev/hls.js's own unit tests). */

#include "037_hls.h"

static uint32_t str_len(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int is_digit(uint8_t c) {
    return c >= (uint8_t) '0' && c <= (uint8_t) '9';
}

/* ---------------------------------------------------------------- */
/* Encoding                                                          */
/* ---------------------------------------------------------------- */

static int append_str(uint8_t *buf, uint32_t *pos, uint32_t size, const char *s) {
    uint32_t n = str_len(s);
    if (*pos + n > size) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        buf[*pos + i] = (uint8_t) s[i];
    }
    *pos += n;
    return 1;
}

static int append_uint(uint8_t *buf, uint32_t *pos, uint32_t size, uint32_t value) {
    char digits[11];
    uint32_t n = 0;
    if (value == 0u) {
        digits[n++] = '0';
    } else {
        char rev[10];
        uint32_t rn = 0;
        while (value > 0u) {
            rev[rn++] = (char) ('0' + (value % 10u));
            value /= 10u;
        }
        while (rn > 0u) {
            digits[n++] = rev[--rn];
        }
    }
    digits[n] = '\0';
    return append_str(buf, pos, size, digits);
}

uint32_t hls_build_master_playlist(const hls_master_playlist_t *p, uint8_t *out,
                                   uint32_t out_size) {
    if (p->variant_count == 0u || p->variant_count > HLS_MAX_VARIANTS) {
        return 0;
    }
    uint32_t pos = 0;
    int ok = append_str(out, &pos, out_size, "#EXTM3U\n");
    for (uint32_t i = 0; i < p->variant_count; i++) {
        const hls_variant_t *v = &p->variants[i];
        ok = ok && append_str(out, &pos, out_size, "#EXT-X-STREAM-INF:BANDWIDTH=");
        ok = ok && append_uint(out, &pos, out_size, v->bandwidth);
        ok = ok && append_str(out, &pos, out_size, ",RESOLUTION=");
        ok = ok && append_uint(out, &pos, out_size, v->width);
        ok = ok && append_str(out, &pos, out_size, "x");
        ok = ok && append_uint(out, &pos, out_size, v->height);
        ok = ok && append_str(out, &pos, out_size, ",CODECS=\"");
        ok = ok && append_str(out, &pos, out_size, v->codecs);
        ok = ok && append_str(out, &pos, out_size, "\",NAME=\"");
        ok = ok && append_str(out, &pos, out_size, v->name);
        ok = ok && append_str(out, &pos, out_size, "\"\n");
        ok = ok && append_str(out, &pos, out_size, v->uri);
        ok = ok && append_str(out, &pos, out_size, "\n");
    }
    return ok ? pos : 0u;
}

uint32_t hls_build_media_playlist(const hls_media_playlist_t *p, uint8_t *out, uint32_t out_size) {
    if (p->segment_count == 0u || p->segment_count > HLS_MAX_SEGMENTS) {
        return 0;
    }
    uint32_t pos = 0;
    int ok = append_str(out, &pos, out_size, "#EXTM3U\n");
    ok = ok && append_str(out, &pos, out_size, "#EXT-X-VERSION:");
    ok = ok && append_uint(out, &pos, out_size, p->version);
    ok = ok && append_str(out, &pos, out_size, "\n#EXT-X-TARGETDURATION:");
    ok = ok && append_uint(out, &pos, out_size, p->target_duration);
    ok = ok && append_str(out, &pos, out_size, "\n");
    for (uint32_t i = 0; i < p->segment_count; i++) {
        ok = ok && append_str(out, &pos, out_size, "#EXTINF:");
        ok = ok && append_uint(out, &pos, out_size, p->segments[i].duration_seconds);
        ok = ok && append_str(out, &pos, out_size, "\n");
        ok = ok && append_str(out, &pos, out_size, p->segments[i].uri);
        ok = ok && append_str(out, &pos, out_size, "\n");
    }
    ok = ok && append_str(out, &pos, out_size, "#EXT-X-ENDLIST\n");
    return ok ? pos : 0u;
}

/* ---------------------------------------------------------------- */
/* Decoding                                                          */
/* ---------------------------------------------------------------- */

static int matches_at(const uint8_t *buf, uint32_t pos, uint32_t end, const char *s) {
    uint32_t n = str_len(s);
    if (pos + n > end) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (buf[pos + i] != (uint8_t) s[i]) {
            return 0;
        }
    }
    return 1;
}

/* Finds the next real newline-terminated line at or after `start`,
 * within `end`. Returns 1 and sets [*out_line_start, *out_line_end) to
 * its own content (excluding the '\n') and *out_next to just after it,
 * or 0 if `start` is already at `end` (no more lines). Refuses (also
 * returns 0) if a non-empty remainder is never newline-terminated. */
static int next_line(const uint8_t *buf, uint32_t start, uint32_t end, uint32_t *out_line_start,
                     uint32_t *out_line_end, uint32_t *out_next) {
    if (start >= end) {
        return 0;
    }
    uint32_t pos = start;
    while (pos < end && buf[pos] != (uint8_t) '\n') {
        pos++;
    }
    if (pos >= end) {
        return 0; /* the real last line was never newline-terminated */
    }
    *out_line_start = start;
    *out_line_end = pos;
    *out_next = pos + 1u;
    return 1;
}

static int parse_uint_span(const uint8_t *buf, uint32_t start, uint32_t end, uint32_t *out) {
    if (start >= end) {
        return 0;
    }
    uint32_t v = 0;
    for (uint32_t i = start; i < end; i++) {
        if (!is_digit(buf[i])) {
            return 0;
        }
        uint32_t d = (uint32_t) (buf[i] - (uint8_t) '0');
        if (v > (0xFFFFFFFFu - d) / 10u) {
            return 0;
        }
        v = v * 10u + d;
    }
    *out = v;
    return 1;
}

static int copy_text_span(const uint8_t *buf, uint32_t start, uint32_t end, char *dst,
                          uint32_t dst_size) {
    uint32_t n = end - start;
    if (n + 1u > dst_size) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = (char) buf[start + i];
    }
    dst[n] = '\0';
    return 1;
}

/* Finds one attribute's own value span within an #EXT-X-STREAM-INF
 * line's own real comma-separated attribute list, respecting real
 * quoted-string values (037_hls.h: CODECS's own real value contains a
 * comma that must NOT be treated as an attribute separator). Returns
 * 1 and sets [*out_start, *out_end) to the value (quotes stripped, if
 * any), or 0 if `name` is not present on this line. */
static int find_attribute(const uint8_t *buf, uint32_t line_start, uint32_t line_end,
                         const char *name, uint32_t *out_start, uint32_t *out_end) {
    uint32_t name_len = str_len(name);
    uint32_t pos = line_start;
    while (pos < line_end) {
        /* Every token, whether it is the one being searched for or
         * one being skipped past on the way to it, has its own value
         * span worked out identically -- and, critically, a QUOTED
         * value's own embedded commas (037_hls.h: CODECS's own real
         * value contains one) are skipped over the same way whether
         * or not this token turns out to be the match. Conflating
         * "is this the requested attribute" with "does this token's
         * own value happen to be quoted" was this file's own earlier
         * bug: it only tracked quotes for the token being matched,
         * so skipping PAST an earlier quoted token (CODECS) while
         * searching for a later one (NAME) mis-split on the comma
         * inside CODECS's own value. */
        uint32_t eq = pos;
        while (eq < line_end && buf[eq] != (uint8_t) '=') {
            eq++;
        }
        if (eq >= line_end) {
            return 0; /* a real token always has its own '=' */
        }
        int is_this_one = (eq - pos == name_len) && matches_at(buf, pos, line_end, name);
        uint32_t value_start = eq + 1u;
        uint32_t value_end = value_start;
        uint32_t token_end;
        if (value_start < line_end && buf[value_start] == (uint8_t) '"') {
            value_start++;
            value_end = value_start;
            while (value_end < line_end && buf[value_end] != (uint8_t) '"') {
                value_end++;
            }
            if (value_end >= line_end) {
                return 0; /* a real quoted value always has its own closing quote */
            }
            token_end = value_end + 1u; /* past the closing quote */
        } else {
            while (value_end < line_end && buf[value_end] != (uint8_t) ',') {
                value_end++;
            }
            token_end = value_end;
        }
        if (is_this_one) {
            *out_start = value_start;
            *out_end = value_end;
            return 1;
        }
        pos = token_end;
        if (pos < line_end && buf[pos] == (uint8_t) ',') {
            pos++;
        }
    }
    return 0;
}

int hls_parse_master_playlist(const uint8_t *buf, uint32_t len, hls_master_playlist_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t ls, le, pos;
    if (!next_line(buf, 0, len, &ls, &le, &pos) || !matches_at(buf, ls, le, "#EXTM3U") ||
        le - ls != 7u) {
        return 0;
    }
    uint32_t count = 0;
    while (pos < len && count < HLS_MAX_VARIANTS) {
        if (!next_line(buf, pos, len, &ls, &le, &pos)) {
            return 0;
        }
        if (!matches_at(buf, ls, le, "#EXT-X-STREAM-INF:")) {
            return 0; /* only this one real playlist shape is supported */
        }
        hls_variant_t *v = &out->variants[count];
        /* Attribute scanning starts right after the tag's own literal
         * "#EXT-X-STREAM-INF:" prefix, matched just above -- an
         * earlier version of this function passed the WHOLE line
         * (prefix included) to find_attribute(), which then read
         * "#EXT-X-STREAM-INF:BANDWIDTH" as one single, unmatched
         * attribute name and refused every real master playlist
         * outright, caught by this chapter's own native test before
         * ever reaching the kernel. */
        uint32_t attrs_start = ls + str_len("#EXT-X-STREAM-INF:");
        uint32_t vs, ve;
        if (!find_attribute(buf, attrs_start, le, "BANDWIDTH", &vs, &ve) ||
            !parse_uint_span(buf, vs, ve, &v->bandwidth)) {
            return 0;
        }
        if (!find_attribute(buf, attrs_start, le, "RESOLUTION", &vs, &ve)) {
            return 0;
        }
        uint32_t x = vs;
        while (x < ve && buf[x] != (uint8_t) 'x') {
            x++;
        }
        if (x >= ve || !parse_uint_span(buf, vs, x, &v->width) ||
            !parse_uint_span(buf, x + 1u, ve, &v->height)) {
            return 0;
        }
        if (!find_attribute(buf, attrs_start, le, "CODECS", &vs, &ve) ||
            !copy_text_span(buf, vs, ve, v->codecs, sizeof(v->codecs))) {
            return 0;
        }
        if (!find_attribute(buf, attrs_start, le, "NAME", &vs, &ve) ||
            !copy_text_span(buf, vs, ve, v->name, sizeof(v->name))) {
            return 0;
        }
        if (!next_line(buf, pos, len, &ls, &le, &pos) ||
            !copy_text_span(buf, ls, le, v->uri, sizeof(v->uri))) {
            return 0; /* a real variant's own URI line must follow */
        }
        count++;
    }
    if (count == 0u || pos != len) {
        return 0; /* no variants at all, or real trailing bytes -- both refused */
    }
    out->variant_count = count;
    return 1;
}

int hls_parse_media_playlist(const uint8_t *buf, uint32_t len, hls_media_playlist_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }
    uint32_t ls, le, pos;
    if (!next_line(buf, 0, len, &ls, &le, &pos) || !matches_at(buf, ls, le, "#EXTM3U") ||
        le - ls != 7u) {
        return 0;
    }
    if (!next_line(buf, pos, len, &ls, &le, &pos) || !matches_at(buf, ls, le, "#EXT-X-VERSION:") ||
        !parse_uint_span(buf, ls + str_len("#EXT-X-VERSION:"), le, &out->version)) {
        return 0;
    }
    if (!next_line(buf, pos, len, &ls, &le, &pos) ||
        !matches_at(buf, ls, le, "#EXT-X-TARGETDURATION:") ||
        !parse_uint_span(buf, ls + str_len("#EXT-X-TARGETDURATION:"), le, &out->target_duration)) {
        return 0;
    }
    uint32_t count = 0;
    while (count < HLS_MAX_SEGMENTS) {
        if (!next_line(buf, pos, len, &ls, &le, &pos)) {
            return 0;
        }
        if (matches_at(buf, ls, le, "#EXT-X-ENDLIST") && le - ls == 14u) {
            break; /* the real, required end of a VOD media playlist */
        }
        if (!matches_at(buf, ls, le, "#EXTINF:")) {
            return 0;
        }
        hls_segment_t *seg = &out->segments[count];
        if (!parse_uint_span(buf, ls + str_len("#EXTINF:"), le, &seg->duration_seconds)) {
            return 0;
        }
        if (!next_line(buf, pos, len, &ls, &le, &pos) ||
            !copy_text_span(buf, ls, le, seg->uri, sizeof(seg->uri))) {
            return 0; /* a real segment's own URI line must follow */
        }
        count++;
    }
    if (count == 0u || pos != len) {
        return 0; /* no segments at all, or real trailing bytes -- both refused */
    }
    out->segment_count = count;
    return 1;
}

int hls_select_variant(const hls_master_playlist_t *p, uint32_t available_bps) {
    int best = -1;
    uint32_t best_bandwidth = 0;
    for (uint32_t i = 0; i < p->variant_count; i++) {
        if (p->variants[i].bandwidth <= available_bps && p->variants[i].bandwidth >= best_bandwidth) {
            best = (int) i;
            best_bandwidth = p->variants[i].bandwidth;
        }
    }
    return best;
}
