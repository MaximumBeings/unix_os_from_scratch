/* See 037_http.h's own top-of-file comment for the full citation trail
 * (RFC 7233's own official mirrors blocked; a real, live, independent
 * npm `http-server` 14.1.1 run locally instead, its own real observed
 * 200/206/416 behavior captured directly). */

#include "037_http.h"

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

/* ---------------------------------------------------------------- */
/* Requests                                                          */
/* ---------------------------------------------------------------- */

uint32_t http_build_request(const http_request_t *req, uint8_t *out, uint32_t out_size) {
    uint32_t pos = 0;
    int ok = 1;
    ok = ok && append_str(out, &pos, out_size, "GET ");
    ok = ok && append_str(out, &pos, out_size, req->path);
    ok = ok && append_str(out, &pos, out_size, " HTTP/1.1\r\n");
    ok = ok && append_str(out, &pos, out_size, "Host: fictionalcdn.example\r\n");
    if (req->has_range) {
        ok = ok && append_str(out, &pos, out_size, "Range: bytes=");
        ok = ok && append_uint(out, &pos, out_size, req->range_start);
        ok = ok && append_str(out, &pos, out_size, "-");
        if (!req->range_open_ended) {
            ok = ok && append_uint(out, &pos, out_size, req->range_end);
        }
        ok = ok && append_str(out, &pos, out_size, "\r\n");
    }
    ok = ok && append_str(out, &pos, out_size, "\r\n");
    return ok ? pos : 0u;
}

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

/* Finds the first real "\r\n" at or after `start`, within `end`.
 * Returns 1 and sets *out_pos to its own position, or 0 if none. */
static int find_crlf(const uint8_t *buf, uint32_t start, uint32_t end, uint32_t *out_pos) {
    for (uint32_t pos = start; pos + 1u < end; pos++) {
        if (buf[pos] == (uint8_t) '\r' && buf[pos + 1u] == (uint8_t) '\n') {
            *out_pos = pos;
            return 1;
        }
    }
    return 0;
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

int http_parse_request(const uint8_t *buf, uint32_t len, http_request_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }

    if (!matches_at(buf, 0, len, "GET ")) {
        return 0; /* this chapter's own only real supported method */
    }
    out->method[0] = 'G'; out->method[1] = 'E'; out->method[2] = 'T'; out->method[3] = '\0';
    uint32_t path_start = 4u;
    uint32_t path_end = path_start;
    while (path_end < len && buf[path_end] != (uint8_t) ' ') {
        path_end++;
    }
    uint32_t path_len = path_end - path_start;
    if (path_end >= len || path_len + 1u > sizeof(out->path)) {
        return 0;
    }
    for (uint32_t i = 0; i < path_len; i++) {
        out->path[i] = (char) buf[path_start + i];
    }
    out->path[path_len] = '\0';

    if (!matches_at(buf, path_end, len, " HTTP/1.1\r\n")) {
        return 0;
    }
    uint32_t line_end;
    if (!find_crlf(buf, path_end, len, &line_end)) {
        return 0;
    }
    uint32_t cursor = line_end + 2u;

    /* Real headers, one per line, until a real blank line ("\r\n\r\n"
     * -- the SECOND "\r\n" of that pair is what the loop below
     * detects by finding a header line of length 0). This chapter's
     * own only real header it inspects is "Range"; any other header
     * line present is real HTTP but simply skipped over, the same way
     * a real server ignores headers it does not care about. */
    for (;;) {
        uint32_t this_line_end;
        if (!find_crlf(buf, cursor, len, &this_line_end)) {
            return 0; /* headers never terminated -- refused, not guessed at */
        }
        if (this_line_end == cursor) {
            break; /* the real blank line ending the header block */
        }
        if (matches_at(buf, cursor, len, "Range: bytes=")) {
            uint32_t v_start = cursor + str_len("Range: bytes=");
            uint32_t dash = v_start;
            while (dash < this_line_end && buf[dash] != (uint8_t) '-') {
                dash++;
            }
            if (dash >= this_line_end) {
                return 0; /* a real Range header always has the '-' */
            }
            if (!parse_uint_span(buf, v_start, dash, &out->range_start)) {
                return 0;
            }
            out->has_range = 1;
            if (dash + 1u == this_line_end) {
                out->range_open_ended = 1;
            } else {
                if (!parse_uint_span(buf, dash + 1u, this_line_end, &out->range_end)) {
                    return 0;
                }
                if (out->range_end < out->range_start) {
                    return 0; /* a real, structurally invalid range */
                }
            }
        }
        cursor = this_line_end + 2u;
    }
    return 1;
}

/* ---------------------------------------------------------------- */
/* Responses                                                         */
/* ---------------------------------------------------------------- */

uint32_t http_build_response(const http_response_t *resp, uint8_t *out, uint32_t out_size) {
    uint32_t pos = 0;
    int ok = 1;
    switch (resp->status) {
    case HTTP_STATUS_200_OK:
        ok = ok && append_str(out, &pos, out_size, "HTTP/1.1 200 OK\r\n");
        ok = ok && append_str(out, &pos, out_size, "Accept-Ranges: bytes\r\n");
        ok = ok && append_str(out, &pos, out_size, "Content-Length: ");
        ok = ok && append_uint(out, &pos, out_size, resp->body_len);
        ok = ok && append_str(out, &pos, out_size, "\r\n\r\n");
        break;
    case HTTP_STATUS_206_PARTIAL:
        ok = ok && append_str(out, &pos, out_size, "HTTP/1.1 206 Partial Content\r\n");
        ok = ok && append_str(out, &pos, out_size, "Accept-Ranges: bytes\r\n");
        ok = ok && append_str(out, &pos, out_size, "Content-Range: bytes ");
        ok = ok && append_uint(out, &pos, out_size, resp->range_start);
        ok = ok && append_str(out, &pos, out_size, "-");
        ok = ok && append_uint(out, &pos, out_size, resp->range_end);
        ok = ok && append_str(out, &pos, out_size, "/");
        ok = ok && append_uint(out, &pos, out_size, resp->resource_total_len);
        ok = ok && append_str(out, &pos, out_size, "\r\n");
        ok = ok && append_str(out, &pos, out_size, "Content-Length: ");
        ok = ok && append_uint(out, &pos, out_size, resp->body_len);
        ok = ok && append_str(out, &pos, out_size, "\r\n\r\n");
        break;
    case HTTP_STATUS_416_RANGE_NOT_SATISFIABLE:
        /* Matches the real live-observed shape exactly (037_http.h):
         * no Content-Range, a real "Content-Length: 0", no body. */
        ok = ok && append_str(out, &pos, out_size,
                              "HTTP/1.1 416 Range Not Satisfiable\r\n");
        ok = ok && append_str(out, &pos, out_size, "Accept-Ranges: bytes\r\n");
        ok = ok && append_str(out, &pos, out_size, "Content-Length: 0\r\n\r\n");
        return ok ? pos : 0u;
    default:
        return 0;
    }
    if (!ok || pos + resp->body_len > out_size) {
        return 0;
    }
    for (uint32_t i = 0; i < resp->body_len; i++) {
        out[pos + i] = resp->body[i];
    }
    pos += resp->body_len;
    return pos;
}

int http_parse_response(const uint8_t *buf, uint32_t len, http_response_t *out) {
    uint8_t *raw = (uint8_t *) out;
    for (uint32_t i = 0; i < sizeof(*out); i++) {
        raw[i] = 0;
    }

    uint32_t status;
    if (matches_at(buf, 0, len, "HTTP/1.1 200 OK\r\n")) {
        status = 200u;
    } else if (matches_at(buf, 0, len, "HTTP/1.1 206 Partial Content\r\n")) {
        status = 206u;
    } else if (matches_at(buf, 0, len, "HTTP/1.1 416 Range Not Satisfiable\r\n")) {
        status = 416u;
    } else {
        return 0; /* only these 3 real status lines are recognized */
    }
    out->status = (http_status_t) status;

    uint32_t line_end;
    if (!find_crlf(buf, 0, len, &line_end)) {
        return 0;
    }
    uint32_t cursor = line_end + 2u;
    int have_content_length = 0;
    uint32_t content_length = 0;
    int have_content_range = 0;

    for (;;) {
        uint32_t this_line_end;
        if (!find_crlf(buf, cursor, len, &this_line_end)) {
            return 0;
        }
        if (this_line_end == cursor) {
            cursor = this_line_end + 2u; /* past the real blank line */
            break;
        }
        if (matches_at(buf, cursor, len, "Content-Length: ")) {
            uint32_t v_start = cursor + str_len("Content-Length: ");
            if (!parse_uint_span(buf, v_start, this_line_end, &content_length)) {
                return 0;
            }
            have_content_length = 1;
        } else if (matches_at(buf, cursor, len, "Content-Range: bytes ")) {
            uint32_t v_start = cursor + str_len("Content-Range: bytes ");
            uint32_t dash = v_start;
            while (dash < this_line_end && buf[dash] != (uint8_t) '-') {
                dash++;
            }
            uint32_t slash = dash;
            while (slash < this_line_end && buf[slash] != (uint8_t) '/') {
                slash++;
            }
            if (dash >= this_line_end || slash >= this_line_end) {
                return 0;
            }
            if (!parse_uint_span(buf, v_start, dash, &out->range_start) ||
                !parse_uint_span(buf, dash + 1u, slash, &out->range_end) ||
                !parse_uint_span(buf, slash + 1u, this_line_end, &out->resource_total_len)) {
                return 0;
            }
            have_content_range = 1;
        }
        cursor = this_line_end + 2u;
    }

    if (!have_content_length) {
        return 0; /* every one of this chapter's own real responses states it */
    }
    if (status == 206u && !have_content_range) {
        return 0; /* a real 206 always carries Content-Range */
    }
    if (status == 416u) {
        if (content_length != 0u || cursor != len) {
            return 0; /* the real live-observed 416 has no body at all */
        }
        out->body = 0;
        out->body_len = 0;
        return 1;
    }
    if (len - cursor != content_length) {
        return 0; /* the real bytes present must match the stated real length */
    }
    out->body = &buf[cursor];
    out->body_len = content_length;
    return 1;
}
