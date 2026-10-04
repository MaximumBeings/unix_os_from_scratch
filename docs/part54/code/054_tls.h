/* Chapter 54: a TLS 1.3 CLIENT (RFC 8446), small enough to read in one sitting, checked against the handshake of RFC 8448 section 3 ("Simple 1-RTT Handshake").
 *
 *   ClientHello (X25519 key share) ->  <- ServerHello (the server's X25519 share)
 *   both sides compute the same shared secret and run the KEY SCHEDULE: HKDF-Extract and HKDF-Expand-Label chain Early Secret -> Handshake Secret -> Master Secret,
 *   with a running hash (the TRANSCRIPT) of every handshake message mixed into each traffic secret
 *   <- {EncryptedExtensions} {Certificate} {CertificateVerify} {Finished}   (protected with the server's handshake traffic keys, AES-128-GCM)
 *   {Finished} ->   (protected with the client's handshake traffic keys)      then both directions switch to APPLICATION traffic keys
 *   <-> application data records, a NewSessionTicket, close_notify
 *
 * WHAT IT DOES: TLS_AES_128_GCM_SHA256 (0x1301) only; key exchange X25519 only; server authentication by an RSA-PSS (rsa_pss_rsae_sha256, 0x0804) CertificateVerify, whose signature IS verified
 * against the key in the server's leaf certificate (own 2048-bit-capable big-number code); the server's Finished is verified; records are protected and opened with sequence-numbered nonces; every received record is
 * strictly parsed, and any failure sets the alert the protocol would send (t->alert).
 * WHAT IT DOES NOT DO (the chapter page lists the consequences): NO X.509 CHAIN VALIDATION and no host-name check (the leaf certificate is used only to obtain the key that must have signed the handshake: without a trust anchor this
 * proves nothing about WHO the server is), no HelloRetryRequest (refused), no PSK or 0-RTT or resumption, no client certificates, no KeyUpdate, no other cipher suites or groups, no record padding on send, no session ticket storage.
 * Not a secure TLS library: do not use it to protect anything. */
#ifndef TLS_H
#define TLS_H
#include <stdint.h>
#include "054_sha256.h"

enum { TLS_OK = 0, TLS_ERR_ARG = -1, TLS_ERR_STATE = -2, TLS_ERR_RECORD = -3, TLS_ERR_DECRYPT = -4, TLS_ERR_HANDSHAKE = -5, TLS_ERR_VERSION = -6, TLS_ERR_SUITE = -7, TLS_ERR_HRR = -8, TLS_ERR_KEY = -9,
       TLS_ERR_CERT = -10, TLS_ERR_SIG = -11, TLS_ERR_FINISHED = -12, TLS_ERR_SPACE = -13, TLS_ERR_ALERT = -14, TLS_ERR_SEQ = -15, TLS_ERR_CLOSED = -16 };
enum { TLS_ST_START = 0, TLS_ST_WAIT_SH, TLS_ST_WAIT_FLIGHT, TLS_ST_CONNECTED, TLS_ST_CLOSED, TLS_ST_FAILED };
/* alert descriptions (RFC 8446 section 6) the client would send for each failure */
enum { TLS_ALERT_CLOSE_NOTIFY = 0, TLS_ALERT_UNEXPECTED_MESSAGE = 10, TLS_ALERT_BAD_RECORD_MAC = 20, TLS_ALERT_RECORD_OVERFLOW = 22, TLS_ALERT_HANDSHAKE_FAILURE = 40, TLS_ALERT_BAD_CERTIFICATE = 42,
       TLS_ALERT_ILLEGAL_PARAMETER = 47, TLS_ALERT_DECODE_ERROR = 50, TLS_ALERT_DECRYPT_ERROR = 51, TLS_ALERT_PROTOCOL_VERSION = 70, TLS_ALERT_INTERNAL_ERROR = 80, TLS_ALERT_MISSING_EXTENSION = 109, TLS_ALERT_UNSUPPORTED_EXTENSION = 110 };
#define TLS_MAX_PLAIN 16384u
#define TLS_HS_BUF 8192u

typedef struct {
    int state, alert; uint8_t priv[32], pub[32], random[32]; sha256_ctx_t th; /* the transcript hash: every handshake message, in order */
    uint8_t hs_secret[32], master[32], c_hs[32], s_hs[32], c_ap[32], s_ap[32], th_sf[32]; /* secrets; th_sf = transcript hash through the server's Finished */
    uint8_t wkey[16], wiv[12], rkey[16], riv[12]; uint64_t wseq, rseq; /* the keys and sequence numbers in use: w = what we send, r = what we receive */
    uint8_t hs[TLS_HS_BUF]; uint32_t hs_len; uint8_t saw_ee, saw_cert, saw_cv, saw_ccs; uint8_t rsa_n[256]; uint32_t rsa_nlen, rsa_e; uint32_t tickets, records_in, records_out;
} tls_t;

/* primitives, exported so the chapter's tests can check each one against RFC vectors and an independent library */
void tls_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]);
void tls_hkdf_extract(const uint8_t *salt, uint32_t slen, const uint8_t *ikm, uint32_t ilen, uint8_t out[32]);
void tls_hkdf_expand_label(const uint8_t secret[32], const char *label, const uint8_t *ctx, uint32_t clen, uint8_t *out, uint32_t olen);
void tls_derive_secret(const uint8_t secret[32], const char *label, const uint8_t th[32], uint8_t out[32]);
void tls_gcm_seal(const uint8_t key[16], const uint8_t iv[12], const uint8_t *aad, uint32_t alen, const uint8_t *pt, uint32_t plen, uint8_t *out); /* out = ciphertext (plen bytes) then the 16-byte tag */
int tls_gcm_open(const uint8_t key[16], const uint8_t iv[12], const uint8_t *aad, uint32_t alen, const uint8_t *ct, uint32_t clen, uint8_t *pt); /* clen includes the tag; 0 ok, -1 on a bad tag (pt is zeroed) */
int tls_rsa_pss_verify(const uint8_t *n, uint32_t nlen, uint32_t e, const uint8_t *sig, uint32_t slen, const uint8_t mhash[32]); /* RSASSA-PSS, SHA-256, MGF1-SHA-256, 32-byte salt: 0 valid */

void tls_init(tls_t *t);
int tls_client_hello(tls_t *t, const uint8_t random[32], const uint8_t priv[32], const char *sni, uint8_t *rec, uint32_t cap, uint32_t *len);
/* one complete record received from the server; out receives records to send back (the client Finished), app receives decrypted application data */
int tls_receive(tls_t *t, const uint8_t *rec, uint32_t len, uint8_t *out, uint32_t outcap, uint32_t *outlen, uint8_t *app, uint32_t appcap, uint32_t *applen);
int tls_send(tls_t *t, const uint8_t *data, uint32_t n, uint8_t *rec, uint32_t cap, uint32_t *len); /* one application-data record */
int tls_close(tls_t *t, uint8_t *rec, uint32_t cap, uint32_t *len); /* a close_notify alert record */
const char *tls_strerror(int rc);
#endif
