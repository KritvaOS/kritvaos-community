# KritvaOS Regression Strategy

## Purpose

Prevent new runtime work from breaking previously accepted functionality.

## Regression Scope

For each task, regression shall include:

- all previously passing tests affected by the change;
- prior KOS-I2 task tests;
- applicable repository integration/system tests;
- clean build verification;
- `git diff --check`.

## Policy

Any unexpected regression blocks acceptance. A waiver requires explicit architectural/reviewer approval and a documented follow-up issue.

## KOS-I2 Progression

- I2-001 establishes the baseline.
- I2-002 must pass I2-001 regression.
- I2-003 must pass I2-001..002 regression.
- I2-004 must pass I2-001..003 regression.
- I2-005 must pass I2-001..004 regression.
- I2-006 must pass the complete KOS-I2 suite.
