# ADR-0002: Fixed-Point Internal Representation

- Status: PROPOSED
- Date: 2026-08-05

## Context

Contract boundaries represent decimal prices and quantities as strings. Projection calculations and
keys must be exact and deterministic across live and replay environments. Contracts preserve
trailing zeroes and do not impose Projection's internal scale or range limits.

## Proposed direction

Translate validated decimal strings at the boundary into distinct, strongly typed signed 64-bit
`PriceUnits` and `QuantityUnits`. Keep price and quantity scales in an explicit, caller-supplied
`NumericSpec`; a units value does not carry an implicit scale. Prices are greater than zero,
quantities are non-negative, and the positive-quantity parser separately requires greater than zero.

Limit storage scales to 0 through 18. `10^18` fits in `std::int64_t`, the range covers currently
expected Binance precision, and the parser still checks every final units value independently. This
is a Projection limit, not a claim about Contracts syntax. Changing or extending the storage-scale
range later is a compatibility decision requiring explicit review.

Convert only when exact. A source with fewer fractional digits is zero-padded with checked
arithmetic. A source with more fractional digits is accepted only when every removed digit is zero.
No parse or format path rounds. Successful parsing retains `source_fraction_digits`, which lets the
formatter reconstruct the canonical source spelling and its trailing zeroes without storing the
source string.

Use prechecked signed 64-bit multiplication and addition; do not rely on overflow followed by
detection. Syntax errors take precedence over exact-scale failures, followed by overflow and then
the price/positive-quantity zero constraint. Details and stable error codes are specified in
`docs/M1_NUMERIC_SEMANTICS.md`.

## Alternatives

Binary floating point is rejected because it cannot represent all decimal contract values exactly
and would make equality and replay results dependent on rounding behavior. Decimal floating point
is rejected because it would add compiler and ABI portability constraints while still requiring
explicit precision and rounding policy. General decimal and multiprecision libraries are rejected
for M1 because checked `std::int64_t` arithmetic completely implements the approved 0-to-18 scale
range without a new production dependency or a larger public API.

Storing strings internally is rejected because numeric comparison and later deterministic book
operations require a canonical numeric representation. Unsigned storage is rejected to avoid
underflow and mixed signed comparisons and to share checked arithmetic conventions with future
derived values. Implicit or per-value scale is rejected in favor of an explicit numeric context.

## Consequences

Projection has a narrower representable range than the Contracts grammar and rejects valid contract
text that does not fit the chosen storage scale or `std::int64_t`. Callers must provide the correct
scale and preserve it with the units values. Exact conversion and retained source precision make
live and replay formatting deterministic.

M1 does not introduce tick-size or step-size validation: the fixed Contracts baseline has no symbol
filter contract, and inventing one would expand the milestone. A future scale-range change affects
accepted inputs, storage compatibility, and downstream assumptions and therefore cannot be made
silently.

## Acceptance condition

This ADR remains PROPOSED until the M1 implementation, boundary tests, property tests, fuzz smoke,
and installed-consumer verification pass. It becomes ACCEPTED only in the final documentation
commit; M1 itself remains in progress pending external review.
