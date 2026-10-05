# I4-002 Acceptance Criteria

- [x] All requirements traced to I4-002 are implemented and evidenced.
- [x] I3 contracts remain unchanged.
- [x] Debug and Release builds pass.
- [x] Unit/integration/sanity tests pass.
- [x] Full KOS-I2 regression passes.
- [x] Full KOS-I3 regression passes.
- [x] ASan passes.
- [x] UBSan passes.
- [x] Applicable mutation/fault-injection checks pass.

- [x] Review checklist/evidence is complete.
- [x] One atomic commit SHA is recorded: 52218f4.
- [x] Core R1.0 source is unchanged.

## Evidence (I4-002)

Deliverable: `hardware/transport/` `frame.hpp`/`frame.cpp` (header validation in the specification order, bounded payload view, frame encoder) and `codec.hpp`/`codec.cpp` (`ByteWriter`, `ByteReader`, string validators, one typed payload struct per message with encode and decode), plus `tests/hardware/support/codec_samples.hpp` and four test programs.

Architect constraints (from the I4-001 final review) and where they are met:

| Constraint | Evidence |
|---|---|
| No Core or I3 contract change; codec independent of transport, remote proxies and hardware; no raw struct serialization; byte order independent of the host | `kritva_hardware_transport` links only `kritva_core` (configure-time check); `frame_test` pins every header byte at its offset, little-endian; `encode_frame` with a byte-swapped field is a caught mutation |
| Decoder never reads outside its bounded span; payload view exactly `payload_length` | `decode_frame` returns a subspan and the fuzz test asserts the view stays inside the input; ASan clean; removing the reader's remaining-bytes check or the string's remaining-length check is caught (the latter by ASan) |
| No allocation before untrusted lengths are validated | `test_hostile_lengths_do_not_drive_allocation` replaces `operator new` and asserts the largest request stays at most 1 KiB while decoding payloads that claim 65535 devices, endpoints, capabilities, settings or bytes |
| Exact consumption in every decoder | `codec_truncation_test`: for every message sample the exact payload passes, every strict prefix is `BAD_PAYLOAD` and trailing bytes (1, 7 and a doubled payload) are `BAD_PAYLOAD`; removing the check is caught |
| Strict validation (finite f64, UTF-8, names, enums, status, ids, duplicates, header_length, flags, reserved, version, sizes) | `codec_test`, `frame_test` (each header step alone and the first failing step reported when several fail) |
| Discovery size computed before encoding; frame bound before item limits | `encoded_size`; `test_discovery_capacity_and_the_frame_bound`: the 64 x 4 maximum is exactly 43524 bytes and round trips, 16 capabilities per endpoint exceeds one frame and is refused before output |
| Cross-field corruption and the codec boundary stated and tested | `test_cross_field_corruption_and_the_codec_boundary` (payload_length and header_length are frame errors; correlation, session, sequence and response-type mismatches are session-layer and decode as received; status/message layout is a codec check) |
| Deterministic tests only | `codec_fuzz_test` uses fixed seeds (xorshift64*), no clock or environment; every accepted mutation re-encodes to identical bytes (canonical) |

- Build: clean Debug and Release, 0 warnings from KOS-I2/I3/I4 code.
- Tests: `kritva_frame_unit`, `kritva_codec_unit`, `kritva_codec_truncation_unit`, `kritva_codec_fuzz_unit` PASS; truncation is exhaustive at every byte offset for every payload and frame up to 2 KiB and sampled (first and last 256 offsets and a stride of 97) for the single 43 KiB maximum discovery sample.
- Regression: `ctest` 123/123 PASS in Debug and Release (76 Core + 18 KOS-I2 + 21 KOS-I3 + 8 I4); `make check` PASS; `git diff --check` clean.
- ASan and UBSan: all I4 unit tests clean under `-fsanitize=address,undefined`, and the fuzz test under each sanitizer alone. ASan found one dangling `std::span` in two of my tests (a view into a temporary frame buffer); fixed in the tests, not in the product.
- Mutation: 28 mutations of the frame, codec and contract code: 26 caught by the tests; the 27th (the string's remaining-length check) is caught by ASan (heap-buffer-overflow); the 28th (the payload view not bounded) is an equivalent mutant, because the earlier length-mismatch check already makes the view exactly the payload.
- I3 contracts, Core R1.0 and the I4-001 contract unchanged (the new code only uses the I4-001 constants).
