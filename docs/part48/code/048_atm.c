/* See 048_atm.h's own top-of-file comment: the account store and
 * dispense sequencing here are this book's own invented design, not a
 * real bank's or vendor's implementation. */

#include "048_atm.h"

void atm_cassette_init(atm_denom_t cassette[ATM_NUM_DENOMS]) {
    static const uint32_t values[ATM_NUM_DENOMS] = {10000u, 5000u, 2000u, 1000u};
    static const uint32_t counts[ATM_NUM_DENOMS] = {20u, 20u, 40u, 40u};
    for (uint32_t i = 0; i < ATM_NUM_DENOMS; i++) {
        cassette[i].value_cents = values[i];
        cassette[i].count = counts[i];
    }
}

int atm_dispense_plan(atm_denom_t cassette[ATM_NUM_DENOMS], uint32_t amount_cents, atm_dispense_plan_t *out_plan) {
    uint32_t remaining = amount_cents;
    uint32_t take[ATM_NUM_DENOMS];
    for (uint32_t i = 0; i < ATM_NUM_DENOMS; i++) {
        uint32_t max_by_amount = remaining / cassette[i].value_cents;
        uint32_t n = (max_by_amount < cassette[i].count) ? max_by_amount : cassette[i].count;
        take[i] = n;
        remaining -= n * cassette[i].value_cents;
    }
    if (remaining != 0u) {
        return 0; /* refused outright -- cassette untouched */
    }
    for (uint32_t i = 0; i < ATM_NUM_DENOMS; i++) {
        cassette[i].count -= take[i];
        out_plan->count[i] = take[i];
    }
    return 1;
}

atm_account_t *atm_find_account(atm_account_t *accounts, uint32_t n, const uint8_t *pan, uint32_t pan_len) {
    for (uint32_t i = 0; i < n; i++) {
        if (accounts[i].pan_len != pan_len) {
            continue;
        }
        int match = 1;
        for (uint32_t j = 0; j < pan_len; j++) {
            if (accounts[i].pan[j] != pan[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            return &accounts[i];
        }
    }
    return 0;
}

int atm_withdraw(atm_account_t *acct, uint32_t amount_cents) {
    if (amount_cents > acct->balance_cents) {
        return 0;
    }
    acct->balance_cents -= amount_cents;
    return 1;
}
