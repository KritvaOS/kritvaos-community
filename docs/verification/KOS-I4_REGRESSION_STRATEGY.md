# KOS-I4 Regression Strategy

1. Pin/consume Core R1.0 without source changes.
2. Run complete KOS-I2 tests unchanged.
3. Run complete KOS-I3 tests unchanged, including mock actuator safety behavior.
4. Run all I4 tests.
5. Repeat under ASan and UBSan.
6. Repeat deterministic transport and protocol fault-injection cases.
7. Verify fresh clone/container.
8. Record exact totals and environment in KOS-I4_VERIFICATION.md.

I4 tests augment; they do not replace or weaken earlier milestone tests.
