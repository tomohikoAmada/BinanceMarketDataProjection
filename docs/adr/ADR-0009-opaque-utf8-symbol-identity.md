# ADR-0009: Opaque Strict-UTF-8 Symbol Identity

- Status: ACCEPTED
- Date: 2026-08-23

## Context

The M4 ProtoAdapter and the independent M5 R4 reference validate the `symbol` field at the wire
boundary. The Contracts Protobuf wire schema defines this field as a `string`; that wire schema does
not define an ASCII uppercase/digit alphabet, a two-to-twenty-byte range, exchange membership, or
case conversion. Any richer Python-domain validation is a separate Contracts follow-up and is not
changed by this ADR.
The previous implementation nevertheless imposed that local grammar. The current official Binance
Spot documentation records that symbol names may contain non-ASCII UTF-8, so the local restriction
could reject a valid upstream identity before the Host has a chance to map it to a transport stream.

This ADR corrects that unsupported implementation restriction. It does not change Core, sequence
classification, order-book state, snapshot semantics, or the Contracts schema.

## Official exact evidence

The current Agent Native official sources were retrieved as text-only HTTPS responses at
`2026-08-23T07:22:34.921794+00:00 UTC`. The ephemeral local acquisition manifest is
`/private/tmp/gateway-binance-docs-20260823/manifest.json`; it records
`remote_content_executed=false`. The portable evidence is the source URL, retrieval time, byte
count, and SHA-256 below; the response bodies are not copied into this repository.

| Source | Bytes | SHA-256 |
| --- | ---: | --- |
| [Spot WebSocket streams](https://developers.binance.com/en/docs/products/spot/web-socket-streams.md) | 10,370 | `193aa07cd537b2ccc94662474fb3dda3cb774d550b1e117825919d99f91b725f` |
| [Spot REST API](https://developers.binance.com/en/docs/products/spot/rest-api.md) | 33,310 | `3bfe5526b745c976ae2db7c6bffdee14f10663d5fe326d8aa54c8b5f12968775` |

The Spot WebSocket source explicitly permits non-ASCII UTF-8 symbol names. The REST source is
recorded as the adjacent official symbol/depth evidence source; neither source is a runtime or
build input.

## Decision

At the ProtoAdapter inbound identity boundary, `symbol` is an opaque exact byte identity:

- it must be non-empty;
- it must be strictly valid UTF-8, rejecting isolated continuation bytes, truncated sequences,
  overlong encodings, UTF-16 surrogate code points, and code points above U+10FFFF;
- it rejects NUL, ASCII C0 controls, DEL (`0x7F`), and ASCII whitespace bytes; and
- it is compared to the Host-owned expected identity by exact byte/string equality.

The adapter does not impose an arbitrary length or exchange-specific alphabet, perform Unicode
normalization, check exchange membership, or apply locale case-folding. The Gateway/Host owns
mapping an identity to a venue's lowercase stream name. The independent M5 R4 reference implements
the same contract with separate lead-byte and boundary-table logic; it does not call production
validation helpers.

## Consequences

Valid opaque identities outside the former ASCII 2..20 grammar, including non-ASCII UTF-8 and
symbols longer than 20 bytes, can reach exact identity comparison and snapshot round-trip. Malformed
UTF-8 and unsafe ASCII C0/DEL delimiter bytes still fail closed at the adapter boundary. Symbol
existence, market association, stream naming, and any exchange-specific subscription rules remain
later Host/Gateway responsibilities.

## Rejected alternatives

- Retaining the unsupported ASCII uppercase/digit 2..20 grammar.
- Adding ICU, a Unicode classification framework, normalization, or locale case-folding.
- Treating UTF-8 validity as exchange membership or implementing `exchangeInfo` in the adapter.
- Lowercasing payload identity in the adapter instead of leaving stream routing to Gateway.
- Duplicating the production validator in the independent R4 oracle.

## Compatibility impact

This is an additive acceptance correction for the existing `string symbol` field. ASCII symbols
that satisfy the previous rule retain their exact behavior; symbols previously rejected only by
the unsupported length/alphabet rule may now be accepted if they satisfy this ADR. Exact mismatch,
numeric validation, market-to-policy mapping, sequence semantics, and Contracts field/enum
compatibility are unchanged.

## Acceptance

Accepted for the M4 adapter boundary and M5 R4 reference on 2026-08-23. The implementation and
focused tests are tracked by Projection issue #48 and the linked Gateway integration work; this ADR
does not authorize changes to Core, sequence policy, or the Contracts package.
