/* See 037_budget.h's own top-of-file comment for the citation trail
 * (the real, general categorization/forecast SHAPE; this book's own
 * invented category names, keyword rules, budget limits, and
 * recurring items). */

#include "037_budget.h"

const char *const BUDGET_CATEGORY_NAMES[BUDGET_MAX_CATEGORIES] = {
    "Dining", "Groceries", "Utilities", "Housing", "Income", "Uncategorized",
};

const uint32_t BUDGET_MONTHLY_LIMIT_CENTS[BUDGET_MAX_CATEGORIES] = {
    15000u,  /* Dining: $150.00 */
    40000u,  /* Groceries: $400.00 */
    12000u,  /* Utilities: $120.00 */
    130000u, /* Housing: $1,300.00 */
    0u,      /* Income: not a spending limit */
    0u,      /* Uncategorized: nowhere to budget it against */
};

/* A plain substring search -- this chapter's own categorization rule
 * is "does this real OFX NAME field contain this keyword anywhere",
 * not an exact match. */
static int contains(const char *haystack, const char *needle) {
    for (uint32_t i = 0; haystack[i] != '\0'; i++) {
        uint32_t j = 0;
        while (needle[j] != '\0' && haystack[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

budget_category_t budget_categorize(const char *name) {
    /* This chapter's own fixed, invented keyword table. Checked in a
     * fixed order; the first match wins, though none of this demo's
     * own fictional transaction names is ambiguous enough for order to
     * actually matter. */
    if (contains(name, "COFFEE")) {
        return BUDGET_CAT_DINING;
    }
    if (contains(name, "GROCERY")) {
        return BUDGET_CAT_GROCERIES;
    }
    if (contains(name, "ELECTRIC") || contains(name, "UTILITY")) {
        return BUDGET_CAT_UTILITIES;
    }
    if (contains(name, "RENT")) {
        return BUDGET_CAT_HOUSING;
    }
    if (contains(name, "PAYROLL") || contains(name, "SALARY")) {
        return BUDGET_CAT_INCOME;
    }
    return BUDGET_CAT_UNCATEGORIZED;
}

void budget_summarize(const ofx_statement_t *stmt, budget_summary_t *out) {
    for (uint32_t i = 0; i < BUDGET_MAX_CATEGORIES; i++) {
        out->spent_cents[i] = 0;
    }
    out->income_cents = 0;
    for (uint32_t i = 0; i < stmt->transaction_count; i++) {
        const ofx_transaction_t *t = &stmt->transactions[i];
        budget_category_t cat = budget_categorize(t->name);
        if (t->amount_cents < 0) {
            uint32_t magnitude = (uint32_t) (-t->amount_cents);
            out->spent_cents[cat] += magnitude;
        } else {
            out->income_cents += (uint32_t) t->amount_cents;
        }
    }
}

int budget_forecast_gap(int32_t starting_balance_cents, const budget_recurring_item_t *items,
                        uint32_t n, uint32_t *out_gap_day) {
    if (n == 0u || n > BUDGET_MAX_RECURRING) {
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (items[i].interval_days == 0u) {
            return 0;
        }
    }
    int32_t balance = starting_balance_cents;
    for (uint32_t day = 0; day < BUDGET_FORECAST_DAYS; day++) {
        for (uint32_t i = 0; i < n; i++) {
            const budget_recurring_item_t *it = &items[i];
            if (day >= it->next_in_days && (day - it->next_in_days) % it->interval_days == 0u) {
                balance += it->amount_cents;
            }
        }
        if (balance < 0) {
            *out_gap_day = day;
            return 1;
        }
    }
    *out_gap_day = BUDGET_FORECAST_DAYS;
    return 1;
}
