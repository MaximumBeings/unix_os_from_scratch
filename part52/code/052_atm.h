#ifndef UNIX_OS_052_ATM_H
#define UNIX_OS_052_ATM_H

#include <stdint.h>

/* This chapter's own account store and cash-dispense sequencing --
 * unlike 052_iso8583.h/052_pinblock.h, NEITHER of these is a real
 * external format this book cites. A real ATM's own core banking
 * ledger and cassette-inventory logic belong to whichever bank or
 * processor built it, and are never published; this chapter states
 * plainly that the layout, the specific denominations, and the greedy
 * dispense algorithm below are this book's own invented design, general
 * in shape (every real ATM ultimately breaks a withdrawal amount down
 * into a whole number of the physical bills a real cassette holds), not
 * a real bank's or vendor's own implementation.
 *
 * This chapter's own invented cassette denominations -- four common US
 * bill values, largest first, this book's own choice: */
#define ATM_NUM_DENOMS 4u

typedef struct {
    uint32_t value_cents; /* e.g. 10000 for a $100 bill */
    uint32_t count;       /* how many bills of this value are loaded */
} atm_denom_t;

/* Fills `cassette` with this chapter's own starting inventory: 20 real
 * $100 bills, 20 $50s, 40 $20s, 40 $10s -- $2,000 + $1,000 + $800 +
 * $400 = $4,200 total, this book's own invented starting float. */
void atm_cassette_init(atm_denom_t cassette[ATM_NUM_DENOMS]);

typedef struct {
    uint32_t count[ATM_NUM_DENOMS]; /* how many of each denom to dispense,
                                     * same order as atm_cassette_init() */
} atm_dispense_plan_t;

/* This chapter's own greedy dispense algorithm: from largest to
 * smallest denomination, take as many bills as the cassette currently
 * has and the remaining amount allows, then move to the next smaller
 * denomination. Returns 1 and fills `out_plan`, removing the chosen
 * bills from `cassette` in place, if `amount_cents` can be made exactly
 * from the bills currently available. Returns 0 -- refusing outright,
 * leaving `cassette` completely untouched -- if any nonzero amount
 * remains once every denomination has been tried (amount_cents is not a
 * multiple of the smallest denomination's value, or the cassette does
 * not currently hold enough bills). */
int atm_dispense_plan(atm_denom_t cassette[ATM_NUM_DENOMS], uint32_t amount_cents, atm_dispense_plan_t *out_plan);

#define ATM_MAX_ACCOUNTS 4u
#define ATM_PAN_MAX 19u

typedef struct {
    uint8_t pan[ATM_PAN_MAX];
    uint32_t pan_len;
    uint32_t balance_cents;
} atm_account_t;

/* Returns a pointer to the account whose PAN matches `pan`/`pan_len`
 * exactly, or 0 if none of the first `n` entries of `accounts` do. */
atm_account_t *atm_find_account(atm_account_t *accounts, uint32_t n, const uint8_t *pan, uint32_t pan_len);

/* Debits `amount_cents` from `acct->balance_cents`. Returns 1 and
 * updates the balance, or 0 -- refusing outright, leaving the balance
 * untouched -- if `amount_cents` exceeds the current balance. */
int atm_withdraw(atm_account_t *acct, uint32_t amount_cents);

#endif
