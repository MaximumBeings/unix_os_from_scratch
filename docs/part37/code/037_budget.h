#ifndef UNIX_OS_037_BUDGET_H
#define UNIX_OS_037_BUDGET_H

#include <stdint.h>
#include "037_ofx.h"

/* A real "automatic categorization" engine (matching each transaction's
 * own real OFX NAME field against a small set of keyword rules to
 * assign a spending category) plus a real cash-flow-gap forecast
 * (projecting an account balance forward from a starting point and a
 * set of recurring income/expense items, to find the first future date
 * the balance would go negative) -- both real, general techniques
 * every personal-finance app of this kind uses, computed entirely in
 * integer cents with no floating point and no native 64-bit
 * division/modulo (the same __udivdi3/__umoddi3 link failure Chapters
 * 7, 30, 33, 34, and 35 each document and avoid -- though this
 * module's own arithmetic stays small enough that it never actually
 * needs a 64-bit intermediate; every value here fits a uint32_t/
 * int32_t on its own).
 *
 * What is real and cited (see 037_ofx.h for the message-format
 * citation itself): the general SHAPE of keyword-based transaction
 * categorization and of a forward balance projection are both
 * well-known, general personal-finance-app techniques, not this
 * chapter's own invention. What is this book's own invention, stated
 * plainly, the same honesty discipline Chapters 34/35 applied to
 * their own tables: the specific category names, the specific keyword
 * rules, the specific fictional monthly budget amounts, and the
 * specific fictional recurring income/expense items this chapter's
 * own demo uses. */

#define BUDGET_MAX_CATEGORIES 6u
#define BUDGET_MAX_RECURRING 4u
#define BUDGET_FORECAST_DAYS 60u /* how far ahead this module projects */

typedef enum {
    BUDGET_CAT_DINING = 0,
    BUDGET_CAT_GROCERIES = 1,
    BUDGET_CAT_UTILITIES = 2,
    BUDGET_CAT_HOUSING = 3,
    BUDGET_CAT_INCOME = 4,
    BUDGET_CAT_UNCATEGORIZED = 5
} budget_category_t;

extern const char *const BUDGET_CATEGORY_NAMES[BUDGET_MAX_CATEGORIES];

/* This chapter's own invented monthly budget per category, in cents
 * (BUDGET_CAT_INCOME and BUDGET_CAT_UNCATEGORIZED have no budget --
 * income is not a spending limit, and an uncategorized transaction has
 * nowhere to be budgeted against). */
extern const uint32_t BUDGET_MONTHLY_LIMIT_CENTS[BUDGET_MAX_CATEGORIES];

/* Matches `name` (an OFX STMTTRN's own real NAME field) against this
 * chapter's own fixed keyword rules and returns the category it maps
 * to, or BUDGET_CAT_UNCATEGORIZED if none match. Case-sensitive: every
 * fictional NAME this chapter's own demo uses is already uppercase,
 * matching real OFX convention (the fetched sample's own NAME/MEMO
 * fields are uppercase too). */
budget_category_t budget_categorize(const char *name);

typedef struct {
    uint32_t spent_cents[BUDGET_MAX_CATEGORIES]; /* DEBITs only, magnitude */
    uint32_t income_cents;                        /* CREDITs, magnitude */
} budget_summary_t;

/* Categorizes and sums every transaction in `stmt` into `out_summary`.
 * A DEBIT's own magnitude is added to its category's own spent total;
 * a CREDIT's own magnitude is added to income, regardless of category
 * (this chapter's own stated scope: income is not broken down by
 * category). */
void budget_summarize(const ofx_statement_t *stmt, budget_summary_t *out_summary);

typedef struct {
    int32_t amount_cents; /* negative for an expense, positive for income */
    uint32_t interval_days;
    uint32_t next_in_days; /* days from "today" (day 0) until it next occurs */
    char label[24];
} budget_recurring_item_t;

/* Projects `starting_balance_cents` forward day by day for
 * BUDGET_FORECAST_DAYS days, applying every item in `items[0..n-1]`
 * whenever `next_in_days` (adjusted by however many full
 * `interval_days` periods have elapsed) lands on that day. Writes the
 * first day on which the running balance would go negative into
 * *out_gap_day (day 0 is "today"), or BUDGET_FORECAST_DAYS if it never
 * does within that window, and returns 1. Returns 0 (refusing
 * outright) if `n` is 0 or above BUDGET_MAX_RECURRING, or any item's
 * own `interval_days` is 0. */
int budget_forecast_gap(int32_t starting_balance_cents, const budget_recurring_item_t *items,
                        uint32_t n, uint32_t *out_gap_day);

#endif
