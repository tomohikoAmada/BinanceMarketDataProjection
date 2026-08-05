# Open questions

## O-P001: Repository license selection

No explicit common license was found in the private Contracts or Recorder repositories. Select a
license (or an explicit proprietary policy) before any public distribution. This does not block the
private engineering baseline.

## O-P002: TSan platform coverage

Confirm which hosted and deployment toolchains provide a stable ThreadSanitizer runtime. The
repository keeps a separate TSan preset and reports actual platform results without treating
unsupported runtimes as a pass.

## O-P003: Order-book container performance decision

M2 uses `std::map` as the correctness baseline for bid/ask storage. M5 will compare candidate
containers (flat_map, abseil btree, sorted vectors, etc.) with representative order-book
transcripts. Until benchmark data exists, no third-party container is introduced. The Public API
is isolated from container choice via PIMPL; changing the internal map type does not require API
changes.
