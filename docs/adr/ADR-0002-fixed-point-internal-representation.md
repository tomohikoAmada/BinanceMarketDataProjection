# ADR-0002: Fixed-Point Internal Representation

- Status: PROPOSED
- Date: 2026-08-05

## Context

Contract boundaries represent decimal prices and quantities as strings. Projection calculations and
keys must be exact and deterministic across live and replay environments.

## Proposed direction

Translate validated decimal strings at the boundary into distinct, strongly typed signed 64-bit
integer units. Use checked wider integer intermediates where multiplication or scaling can exceed the
stored width. Do not use floating-point values for price or quantity.

## Deferred validation

M1 will finalize ranges, scales, parsing, rounding/rejection rules, overflow behavior, and compiler
support for wide intermediates. M0 deliberately implements none of these numeric types or behaviors.
