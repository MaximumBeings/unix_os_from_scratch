/* Chapter 47: a double-entry money ledger in integer cents. See 052_ledger.h. */
#include "052_ledger.h"

void led_init(ledger_t *l) {
    for (uint32_t i = 0; i < LED_MAX_ACCTS; i++) {
        l->bal[i] = 0;
    }
    l->n_tx = 0;
}

int led_transfer(ledger_t *l, uint32_t from, uint32_t to, uint32_t cents) {
    if (from >= LED_MAX_ACCTS || to >= LED_MAX_ACCTS || from == to) {
        return LED_ERR_ACCT;
    }
    if (cents == 0 || cents > 100000000u) {
        return LED_ERR_AMOUNT;
    }
    if (from != LED_EXTERNAL && l->bal[from] < (int32_t)cents) {
        return LED_ERR_FUNDS;
    }
    l->bal[from] -= (int32_t)cents;
    l->bal[to] += (int32_t)cents;
    l->n_tx++;
    return LED_OK;
}

int led_deposit(ledger_t *l, uint32_t acct, uint32_t cents) {
    if (acct < LED_FIRST_USER) {
        return LED_ERR_ACCT;
    }
    return led_transfer(l, LED_EXTERNAL, acct, cents);
}

int32_t led_total(const ledger_t *l) {
    int32_t t = 0;
    for (uint32_t i = 0; i < LED_MAX_ACCTS; i++) {
        t += l->bal[i];
    }
    return t;
}

uint32_t led_fee(uint32_t total_cents) {
    if (total_cents > 100000000u) {
        return 0xFFFFFFFFu;
    }
    return (total_cents * 10u + 50u) / 100u + 30u;
}
