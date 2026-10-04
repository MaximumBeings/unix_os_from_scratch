/* Chapter 47: a double-entry money ledger in integer cents. Industrial-grade money handling reduced to three rules this file enforces and tests:
 *   1. Money is an integer number of cents. There is no floating point anywhere in this chapter's money path.
 *   2. Money is only ever MOVED between accounts, never created or destroyed by a transfer. Account 0 is the outside world ("external"); a deposit is a transfer
 *      FROM it, so its balance goes negative by exactly what the platform holds. The sum of ALL balances is therefore always exactly zero -- led_total() -- and the
 *      market layer checks that after EVERY command (the chapter's money-conservation invariant).
 *   3. A non-external account can never go below zero: a transfer that would overdraw it is refused with nothing changed.
 * Accounts: 0 external, 1 escrow (the platform holds a buyer's payment here until the order is released), 2 platform fees, 3.. users (a user's id IS their account number). */
#ifndef LEDGER_H
#define LEDGER_H
#include <stdint.h>
#define LED_MAX_ACCTS 12
#define LED_EXTERNAL 0u
#define LED_ESCROW 1u
#define LED_FEES 2u
#define LED_FIRST_USER 3u
enum { LED_OK = 0, LED_ERR_FUNDS = 1, LED_ERR_ACCT = 2, LED_ERR_AMOUNT = 3 };
typedef struct { int32_t bal[LED_MAX_ACCTS]; uint32_t n_tx; } ledger_t;
void led_init(ledger_t *l);
int led_transfer(ledger_t *l, uint32_t from, uint32_t to, uint32_t cents);
int led_deposit(ledger_t *l, uint32_t acct, uint32_t cents);
int32_t led_total(const ledger_t *l); /* must always be 0 */
/* This chapter's FICTIONAL platform fee on a sale: 10% of the sale total rounded half up, plus 30 cents. (eBay's real final-value-fee schedule varies by category and is not
 * modelled; the point here is exact integer arithmetic, not eBay's price list.) Refuses totals above $1,000,000.00 so the 32-bit multiply below cannot overflow. */
uint32_t led_fee(uint32_t total_cents);
#endif
