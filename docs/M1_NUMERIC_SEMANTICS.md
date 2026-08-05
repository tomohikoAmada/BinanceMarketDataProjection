# M1 Numeric Semantics

## Scope and contract baseline

M1 defines the exact numeric boundary and domain primitives used by later projection milestones. Its
read-only Contracts semantic baseline is commit
`01d76a41929f36d89573159f5f458f9f1e378ada` of BinanceMarketDataContracts. Contracts continue to
publish validated decimal strings. Projection converts those strings to distinct signed 64-bit unit
types; it does not copy or depend on Contracts code or Protobuf definitions.

The public/wire and internal representations are deliberately different:

```text
Public/wire boundary: validated decimal strings
Projection internal:  PriceUnits or QuantityUnits backed by int64_t
Numeric context:      caller-supplied DecimalScale values
Conversion:           exact or rejected
Rounding:             never
```

## Scale and domain invariants

`DecimalScale` admits values from 0 through `kMaxDecimalScale`, inclusive.
`kMaxDecimalScale` is 18 because `10^18` is representable in `std::int64_t` and this range covers the
currently expected Binance price and quantity precision. Every final unit value is independently
checked against `INT64_MAX`. Contracts do not impose this 18-digit storage-scale limit; Projection
therefore has a deliberately narrower internal range and reports an explicit error when a
syntactically valid contract value cannot be represented.

`PriceUnits` stores a value greater than zero. `QuantityUnits` stores a value greater than or equal
to zero. `parse_positive_quantity` additionally requires a quantity greater than zero. Ordinary
quantity parsing permits zero because a later milestone can use zero quantity to represent removal
of a price level. The M1 API has no signed-decimal public type.

Scale belongs to caller-supplied `NumericSpec`, not to an individual units value. A units value must
only be interpreted or compared in a context with the applicable scale.

## Parsing and exact rescale

Accepted text matches `^(0|[1-9][0-9]*)(\.[0-9]+)?$`. Signs, whitespace, leading zeroes, missing
integer or fractional digits, scientific notation, locale-specific separators, non-ASCII digits,
and non-finite spellings are rejected.

The parser retains `source_fraction_digits`, the exact number of fractional digits in the input.
For example, `"1"`, `"1.0"`, and `"1.2300"` retain 0, 1, and 4 respectively. It does not retain a
copy of the input. Given the canonical Contracts grammar, the units, storage scale, and source
fraction digit count are sufficient for the formatter to reconstruct the original text, including
its trailing zeroes.

Rescaling is exact:

- If the source has fewer fractional digits than storage, checked powers of ten append unit zeroes.
- If the counts are equal, units are used directly.
- If the source has more fractional digits than storage, every discarded digit must be zero.
- A discarded non-zero digit returns `InexactScale`; it is never rounded, floored, ceiled, or
  silently truncated.

The parser performs one linear scan, does not allocate, is locale-independent, and is `noexcept`.
It checks multiplication and addition before performing them, so signed integer overflow cannot
occur.

## Errors and precedence

`DecimalErrorCode` is a stable, finite taxonomy:

- `Empty`
- `InvalidSyntax`
- `SignNotAllowed`
- `LeadingZero`
- `MissingFractionDigits`
- `ZeroNotAllowed`
- `InexactScale`
- `Overflow`

`DecimalError::offset` is the zero-based byte offset that identifies the relevant input position.
`Empty`, domain-zero errors, and an overflow caused only by final zero-padding use
`kNoErrorOffset`. An overflow while consuming a digit reports that digit; an inexact rescale reports
the first non-zero discarded digit. Syntax categories have deterministic offsets: a leading sign
reports zero, a leading-zero violation reports the second integer digit, a missing fractional part
reports the decimal point, and other syntax failures report the offending byte.

The parser evaluates error classes in this precedence order:

1. Syntax, including the specific syntax categories above.
2. Exact-scale representability.
3. Signed 64-bit unit overflow.
4. The non-zero constraint for prices or positive quantities.

Within syntax errors, the earliest conclusive grammar violation is returned. This ordering means,
for example, a non-zero discarded digit wins over an otherwise overflowing coefficient, and an
overflow wins over a zero-domain check.

## Formatting

Formatting is deterministic, locale-independent, and never uses scientific notation. It emits no
sign or unnecessary leading zero, emits `0` before a fractional value below one, and omits the
decimal point when the requested fractional digit count is zero.

Increasing output precision appends zeroes. Reducing it requires the stored units to be exactly
divisible by the removed power of ten; otherwise formatting returns `InexactScale`. Fixed-format
convenience functions request the storage scale. Formatters return `std::string`, so normal standard
library allocation and allocation exceptions apply; they do not throw business-validation
exceptions and are intentionally not marked `noexcept`.

For every successful parse, formatting with the retained `source_fraction_digits` reconstructs the
original bytes. Fixed formatting followed by parsing at the same storage scale preserves units.

## Deliberate omissions

M1 does not validate exchange tick size or step size because the Contracts baseline has no symbol
filter contract. It does not add symbol metadata, signed decimals, an order book, or projection
state. M2 may use `PriceUnits`, `QuantityUnits`, and an explicit `NumericSpec` as its numeric
foundation, but M1 does not pre-design or implement the M2 order-book API.
