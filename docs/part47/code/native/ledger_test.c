/* Chapter 47, host-side ledger test (NOT part of the kernel). Build and run:  gcc -O1 -fsanitize=address,undefined -I.. ledger_test.c ../047_ledger.c -o ledger_test && ./ledger_test > ledger_out.txt
 * 1. 2,000,000 random transfers and deposits: the ledger always sums to zero, no user account is ever negative, refused transfers change nothing;
 * 2. overdraft: a transfer larger than the balance is refused with LED_ERR_FUNDS and nothing moves;
 * 3. the fee for every sale total from 0 to 100,000 cents (led_fee), printed for ledger_ref.py to compare against an independent computation with decimal arithmetic. */
#include <stdio.h>
#include <stdlib.h>
#include "047_ledger.h"
static uint32_t rs = 7;
static uint32_t rnd(void) { uint32_t x = rs; x ^= x << 13; x ^= x >> 17; x ^= x << 5; rs = x; return x; }
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); exit(1); } } while (0)
int main(void) {
    ledger_t l; led_init(&l); uint64_t ok = 0, refused = 0;
    for (uint32_t i = 0; i < 2000000; i++) {
        uint32_t from = rnd() % 14, to = rnd() % 14, c = rnd() % 5000; ledger_t before = l;
        int rc = (rnd() % 3 == 0) ? led_deposit(&l, to, c) : led_transfer(&l, from, to, c);
        if (rc != LED_OK) { refused++; for (int k = 0; k < LED_MAX_ACCTS; k++) CHECK(l.bal[k] == before.bal[k], "a refused transfer changed account %d", k); CHECK(l.n_tx == before.n_tx, "n_tx"); }
        else ok++;
        CHECK(led_total(&l) == 0, "ledger total %d", led_total(&l));
        for (uint32_t k = LED_FIRST_USER; k < LED_MAX_ACCTS; k++) CHECK(l.bal[k] >= 0, "account %u negative", k);
        CHECK(l.bal[LED_ESCROW] >= 0 && l.bal[LED_FEES] >= 0, "escrow or fees negative");
    }
    printf("1. 2000000 random transfers/deposits (%llu applied, %llu refused): ledger total 0 after every one, no negative account, refused transfers changed nothing\n", (unsigned long long)ok, (unsigned long long)refused);
    led_init(&l); CHECK(led_deposit(&l, 4, 1000) == LED_OK, "deposit"); ledger_t b = l;
    CHECK(led_transfer(&l, 4, 5, 1001) == LED_ERR_FUNDS && l.bal[4] == b.bal[4] && l.bal[5] == b.bal[5] && l.n_tx == b.n_tx, "overdraft must be refused, nothing moved");
    CHECK(led_transfer(&l, 4, 5, 1000) == LED_OK && l.bal[4] == 0 && l.bal[5] == 1000, "spending the exact balance is allowed");
    CHECK(led_transfer(&l, 4, 5, 1) == LED_ERR_FUNDS, "an empty account cannot pay");
    printf("2. overdraft: a $10.01 transfer from a $10.00 account is refused with nothing moved; spending exactly $10.00 works; the empty account then cannot pay even 1 cent\n");
    for (uint32_t t = 0; t <= 100000; t++) printf("FEE %u %u\n", t, led_fee(t));
    return 0;
}
