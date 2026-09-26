#ifndef UNIX_OS_037_HLS_H
#define UNIX_OS_037_HLS_H

#include <stdint.h>

/* A real HLS (HTTP Live Streaming) playlist encoder/decoder -- both
 * real kinds of `.m3u8` manifest a real adaptive-bitrate player reads:
 * a real MASTER (variant) playlist, listing several quality variants
 * of the same real content, each with its own real bandwidth and
 * resolution; and a real MEDIA (level) playlist, listing one
 * variant's own real timed segments.
 *
 * HLS's own official specification (an IETF RFC, and Apple's own
 * developer documentation) is not something this book could reach
 * directly under this sandbox's network egress policy, the same
 * pattern every blocked-standards-site chapter since Chapter 33 has
 * hit. But GitHub is reachable, and this chapter found something
 * better than a schema summary once again: hls.js, a real,
 * widely-used, open-source HLS client, and its own real unit tests
 * for its own real M3U8 parser -- `tests/unit/loader/m3u8-parser.ts`
 * and `tests/unit/controller/buffer-controller-operations.ts`, in a
 * clone of github.com/video-dev/hls.js, fetched and read in full, the
 * same discipline Chapters 32/34/35/36 applied to NACHA, ACORD, FIX,
 * and OFX. Every real tag below, and the real fixture text quoted
 * here, was read directly out of those two files:
 *
 *   A real master playlist fixture (m3u8-parser.ts):
 *     #EXTM3U
 *     #EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=836280,CODECS="mp4a.40.2,avc1.64001f",RESOLUTION=848x360,NAME="480"
 *     http://proxy-62.dailymotion.com/sec(...)/480/x1u4wjr.m3u8
 *
 *   A real media (level) playlist fixture
 *     (buffer-controller-operations.ts):
 *     #EXTM3U
 *     #EXT-X-VERSION:3
 *     #EXT-X-TARGETDURATION:6
 *     #EXTINF:6
 *     1.seg
 *     #EXTINF:6
 *     2.seg
 *     #EXT-X-ENDLIST
 *
 * The real tags this chapter's own encoder/decoder handles, all
 * OBSERVED directly in those two fixtures: `#EXTM3U` (the real,
 * required first line of every HLS playlist); `#EXT-X-STREAM-INF`
 * (marks a master-playlist variant, with real attributes `BANDWIDTH`,
 * `RESOLUTION`, `CODECS`, and `NAME`, followed by the variant's own
 * playlist URI on the next line); `#EXT-X-VERSION`; `#EXT-X-
 * TARGETDURATION`; `#EXTINF` (a segment's own real duration in
 * seconds, followed by the segment's own URI on the next line); and
 * `#EXT-X-ENDLIST` (marks a real VOD -- not live -- media playlist,
 * whose segment list is now complete).
 *
 * This chapter's own invention, stated plainly: every fictional
 * BANDWIDTH/RESOLUTION/CODECS/NAME value, every fictional segment
 * count and URI, and the adaptive-bitrate SELECTION algorithm this
 * chapter builds on top of a parsed master playlist (see the demo in
 * 037_kmain.c) -- "pick the highest-bandwidth variant that still fits
 * a given available-bandwidth estimate" is a real, common, general ABR
 * strategy real players use, not this book's own invention, but no
 * single real player's own exact algorithm is reproduced here. */

#define HLS_MAX_VARIANTS 4u
#define HLS_MAX_SEGMENTS 8u
#define HLS_MAX_URI_LEN 32u
#define HLS_MAX_CODECS_LEN 32u
#define HLS_MAX_MESSAGE_LEN 1024u

typedef struct {
    uint32_t bandwidth;   /* real EXT-X-STREAM-INF BANDWIDTH, bits/sec */
    uint32_t width, height; /* real RESOLUTION, WxH */
    char codecs[HLS_MAX_CODECS_LEN];
    char name[16];
    char uri[HLS_MAX_URI_LEN];
} hls_variant_t;

typedef struct {
    hls_variant_t variants[HLS_MAX_VARIANTS];
    uint32_t variant_count;
} hls_master_playlist_t;

typedef struct {
    uint32_t duration_seconds; /* real EXTINF value, truncated to whole seconds */
    char uri[HLS_MAX_URI_LEN];
} hls_segment_t;

typedef struct {
    uint32_t version;
    uint32_t target_duration;
    hls_segment_t segments[HLS_MAX_SEGMENTS];
    uint32_t segment_count;
} hls_media_playlist_t;

/* Builds a real master playlist. Returns the encoded length, or 0 if
 * `out_size` is too small, `variant_count` is 0 or above
 * HLS_MAX_VARIANTS, or any text field is too long for its own stated
 * width. */
uint32_t hls_build_master_playlist(const hls_master_playlist_t *playlist, uint8_t *out,
                                   uint32_t out_size);

/* Parses exactly `len` bytes. Returns 1 on success, or 0 -- refusing
 * outright -- if the real `#EXTM3U` first line is missing, any
 * `#EXT-X-STREAM-INF` attribute is malformed, or a variant's own URI
 * line is missing. */
int hls_parse_master_playlist(const uint8_t *buf, uint32_t len,
                              hls_master_playlist_t *out_playlist);

/* Builds a real media (level) playlist ending in a real
 * `#EXT-X-ENDLIST` (this chapter's own scope: VOD only, never a real
 * live/growing playlist). Returns the encoded length, or 0 on the same
 * refusal conditions as hls_build_master_playlist(). */
uint32_t hls_build_media_playlist(const hls_media_playlist_t *playlist, uint8_t *out,
                                  uint32_t out_size);

/* Parses exactly `len` bytes, the same refusal discipline as
 * hls_parse_master_playlist(), plus refusing a playlist missing its
 * own real `#EXT-X-ENDLIST`. */
int hls_parse_media_playlist(const uint8_t *buf, uint32_t len,
                             hls_media_playlist_t *out_playlist);

/* This chapter's own real, general adaptive-bitrate rule (stated
 * above): the highest-BANDWIDTH variant whose own BANDWIDTH does not
 * exceed `available_bps`. Returns the chosen variant's own index, or
 * -1 if `playlist->variant_count` is 0 or every real variant's own
 * BANDWIDTH exceeds `available_bps` (this chapter's own stated
 * refusal: it never silently picks a variant the estimate cannot
 * support). */
int hls_select_variant(const hls_master_playlist_t *playlist, uint32_t available_bps);

#endif
