# Open questions

## O-P001: Repository license selection

No explicit common license was found in the private Contracts or Recorder repositories. Select a
license (or an explicit proprietary policy) before any public distribution. This does not block the
private M0 engineering baseline.

## O-M101: Fixed-point numeric representation

M1 must validate signed range, scaling, overflow policy, parsing behavior, and wide intermediate
arithmetic before introducing numeric domain types.

## O-P002: TSan platform coverage

Confirm which hosted and deployment toolchains provide a stable ThreadSanitizer runtime. M0 keeps a
separate TSan preset and reports actual platform results without treating unsupported runtimes as a
pass.
