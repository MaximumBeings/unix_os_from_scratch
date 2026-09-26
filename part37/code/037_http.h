#ifndef UNIX_OS_037_HTTP_H
#define UNIX_OS_037_HTTP_H

#include <stdint.h>

/* A real HTTP/1.1 request/response encoder/decoder, focused on real
 * byte-range requests (RFC 7233, "HTTP/1.1 Range Requests") -- the
 * real mechanism that lets a video player fetch just the bytes of one
 * segment out of a much larger file, rather than downloading the
 * whole thing first.
 *
 * RFC 7233's own official mirrors -- rfc-editor.org, datatracker.
 * ietf.org, httpwg.org, and a third-party mirror (greenbytes.de) --
 * are all blocked by this sandbox's network egress policy, the same
 * honesty note this book has made about every other blocked standards
 * site since Chapter 33. Search results surfaced the RFC's own quoted
 * example text (a real `Range: bytes=0-499` request; a real response
 * `HTTP/1.1 206 Partial Content` / `Content-Range: bytes
 * 21010-47021/47022`), which is cited here the same weaker way
 * Chapter 33 cited Regulation Z -- but this chapter did something
 * stronger where it could: rather than stop at a citation, it spun up
 * a REAL, live, independent implementation and watched it behave.
 * `http-server` 14.1.1 (a real, widely-used npm package, run locally
 * in this sandbox against a real 2000-byte test file) produced, for
 * real, on an actual TCP connection:
 *
 *   Range: bytes=0-499       -> HTTP/1.1 206 Partial Content
 *                                Content-Range: bytes 0-499/2000
 *                                Content-Length: 500
 *   Range: bytes=1500-       -> HTTP/1.1 206 Partial Content
 *                                Content-Range: bytes 1500-1999/2000
 *                                Content-Length: 500
 *   Range: bytes=9000-9999   -> HTTP/1.1 416 Range Not Satisfiable
 *                                Content-Length: 0
 *   (no Range header at all) -> HTTP/1.1 200 OK
 *                                Content-Length: 2000
 *
 * -- both request forms (a closed range and an open-ended "from this
 * byte to the end" range), the real `Content-Range: bytes
 * START-END/TOTAL` response format, the real distinction between a
 * satisfiable range (206) and one entirely past the end of the real
 * resource (416, with no body), and every real response carrying
 * `Accept-Ranges: bytes` to advertise range support in the first
 * place -- all captured directly, not assumed from a spec this
 * sandbox could not read. `037_http.c`'s own encoder/decoder matches
 * that real, live-observed behavior field-for-field.
 *
 * This chapter's own scope, confirmed before any code was written,
 * deliberately excludes Chapter 30's own AES-128-CBC + HMAC-SHA256
 * construction: this is not a payment chapter, and this file focuses
 * purely on the real HTTP/manifest mechanics themselves. Every
 * request/response this chapter's own demo builds travels in plain
 * text over the same RTL8139 hardware loopback path used since
 * Chapter 27, with no encryption at all -- stated plainly as a scope
 * choice, not an oversight. */

#define HTTP_MAX_PATH_LEN 64u
/* A real, hard stated limit, not an arbitrary round number: this
 * chapter's own demo (037_kmain.c) has no TCP segmentation at all --
 * one real Ethernet frame carries one whole HTTP message, headers and
 * body together -- and the real RTL8139 hardware this book has used
 * since Chapter 27 refuses any frame above 1792 bytes
 * (RTL8139_MAX_FRAME). This value is sized to leave real room for a
 * message's own 16-byte Ethernet framing plus its own real headers on
 * top of whatever body it carries, comfortably under that hardware
 * ceiling -- an early version of this chapter tried to serve a
 * 2048-byte video segment through a 2048-byte message buffer with no
 * room left for its own headers at all, caught immediately by
 * `http_build_response()`'s own honest refusal rather than a silent
 * truncation. */
#define HTTP_MAX_MESSAGE_LEN 1024u

typedef struct {
    char method[8];  /* "GET", this chapter's own only real method */
    char path[HTTP_MAX_PATH_LEN];
    int has_range;
    uint32_t range_start;
    int range_open_ended; /* 1 for "bytes=START-" (no end given) */
    uint32_t range_end;   /* only meaningful if !range_open_ended */
} http_request_t;

/* Builds a real "GET <path> HTTP/1.1\r\nHost: ...\r\n[Range: bytes=
 * ...\r\n]\r\n" request. Returns the encoded length, or 0 if `out_size`
 * is too small or `path` is too long. */
uint32_t http_build_request(const http_request_t *req, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a request. Returns 1 on success, or 0
 * -- refusing outright -- if the request line is not real
 * "METHOD SPACE PATH SPACE HTTP/1.1\r\n", the method is not "GET", a
 * present Range header does not match the real "bytes=N-[M]" shape,
 * or the headers are not terminated by a real blank line ("\r\n\r\n"). */
int http_parse_request(const uint8_t *buf, uint32_t len, http_request_t *out_req);

typedef enum {
    HTTP_STATUS_200_OK = 200,
    HTTP_STATUS_206_PARTIAL = 206,
    HTTP_STATUS_416_RANGE_NOT_SATISFIABLE = 416,
} http_status_t;

typedef struct {
    http_status_t status;
    uint32_t range_start;       /* only meaningful for 206 */
    uint32_t range_end;         /* only meaningful for 206 */
    uint32_t resource_total_len; /* the real resource's own full length --
                                  * the "/T" in a real 206's own Content-Range;
                                  * unused for 200 (body_len already is the
                                  * total) and for 416 (the real live server
                                  * never sent a Content-Range on it at all) */
    const uint8_t *body;        /* not owned; NULL for 416 */
    uint32_t body_len;          /* the real Content-Length: range_end-range_start+1
                                 * for 206, the whole resource's length for 200,
                                 * 0 for 416 */
} http_response_t;

/* Builds a real response matching the live-observed shape above:
 * 200 gets "Accept-Ranges: bytes"/Content-Length/body; 206 gets
 * "Accept-Ranges: bytes"/"Content-Range: bytes S-E/T"/Content-Length/
 * body; 416 gets "Accept-Ranges: bytes"/Content-Length: 0, no body,
 * no Content-Range -- exactly what the real live server returned, not
 * a value this book assumed a 416 "ought" to carry. Returns the
 * encoded length, or 0 if `out_size` is too small. */
uint32_t http_build_response(const http_response_t *resp, uint8_t *out, uint32_t out_size);

/* Parses exactly `len` bytes of a response into `out_resp`, with
 * `out_resp->body` pointing back into `buf` itself (never copied).
 * Returns 1 on success, or 0 -- refusing outright -- on an
 * unrecognized status line, a malformed Content-Range, a
 * Content-Length that does not match the real bytes actually present
 * after the header block's own blank-line terminator, or missing
 * headers. */
int http_parse_response(const uint8_t *buf, uint32_t len, http_response_t *out_resp);

#endif
