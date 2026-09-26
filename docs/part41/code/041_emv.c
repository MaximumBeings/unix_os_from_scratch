/* See 041_emv.h's own top-of-file comment for the citation of every
 * real tag/response shape used here, and the honesty note on
 * emv_generate_cryptogram()'s own invented substitute. */

#include "041_emv.h"
#include "041_hmac.h"

static void put_input_bytes(const emv_gac_input_t *in, uint8_t *buf, uint32_t *pos) {
    for (uint32_t i = 0; i < 6u; i++) buf[(*pos)++] = in->amount_authorized[i];
    for (uint32_t i = 0; i < 2u; i++) buf[(*pos)++] = in->transaction_currency_code[i];
    for (uint32_t i = 0; i < 3u; i++) buf[(*pos)++] = in->transaction_date[i];
    buf[(*pos)++] = in->transaction_type;
    for (uint32_t i = 0; i < 4u; i++) buf[(*pos)++] = in->unpredictable_number[i];
    for (uint32_t i = 0; i < 2u; i++) buf[(*pos)++] = in->terminal_country_code[i];
    for (uint32_t i = 0; i < 5u; i++) buf[(*pos)++] = in->tvr[i];
    buf[(*pos)++] = (uint8_t) (in->atc >> 8);
    buf[(*pos)++] = (uint8_t) in->atc;
}

void emv_generate_cryptogram(const emv_gac_input_t *in, uint8_t cid, const uint8_t *key,
                              uint32_t key_len, uint8_t out_ac[8]) {
    uint8_t buf[6 + 2 + 3 + 1 + 4 + 2 + 5 + 2 + 1];
    uint32_t pos = 0;
    put_input_bytes(in, buf, &pos);
    buf[pos++] = cid;
    uint8_t tag[HMAC_SHA256_TAG_SIZE];
    hmac_sha256(key, key_len, buf, pos, tag);
    for (uint32_t i = 0; i < 8u; i++) {
        out_ac[i] = tag[i];
    }
}

uint8_t emv_terminal_risk_management(const uint8_t tvr[5], uint32_t amount_cents,
                                      uint32_t floor_limit_cents) {
    int offline_auth_not_performed = (tvr[0] & 0x80u) != 0u;
    int hotlisted = (tvr[0] & 0x08u) != 0u;
    if (offline_auth_not_performed || hotlisted || amount_cents > floor_limit_cents) {
        return EMV_CID_ARQC;
    }
    return EMV_CID_TC;
}

uint32_t emv_build_gac_response(const emv_gac_response_t *resp, uint8_t *out, uint32_t out_size) {
    tlv_list_t list;
    list.count = 1;
    tlv_object_t *o = &list.objects[0];
    o->tag[0] = 0x80u;
    o->tag_len = 1;
    uint32_t pos = 0;
    o->value[pos++] = resp->cid;
    o->value[pos++] = (uint8_t) (resp->atc >> 8);
    o->value[pos++] = (uint8_t) resp->atc;
    for (uint32_t i = 0; i < 8u; i++) {
        o->value[pos++] = resp->ac[i];
    }
    for (uint32_t i = 0; i < EMV_IAD_LEN; i++) {
        o->value[pos++] = resp->iad[i];
    }
    o->value_len = (uint8_t) pos;
    return tlv_build(&list, out, out_size);
}

int emv_parse_gac_response(const tlv_list_t *list, emv_gac_response_t *out) {
    uint8_t tag80 = 0x80u;
    const tlv_object_t *o = tlv_find(list, &tag80, 1);
    if (o == 0 || o->value_len != 1u + 2u + 8u + EMV_IAD_LEN) {
        return 0;
    }
    uint32_t pos = 0;
    out->cid = o->value[pos++];
    out->atc = ((uint16_t) o->value[pos] << 8) | o->value[pos + 1u];
    pos += 2u;
    for (uint32_t i = 0; i < 8u; i++) {
        out->ac[i] = o->value[pos++];
    }
    for (uint32_t i = 0; i < EMV_IAD_LEN; i++) {
        out->iad[i] = o->value[pos++];
    }
    return 1;
}
