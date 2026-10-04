# KOS-I2 Git Commit Procedure

## Pre-commit gate

```text
Implementation → Unit → Integration → Sanity → Regression → Review → Diff Review → Commit
```

Run at minimum:

```bash
git status
git diff --check
git diff
```

## Commit granularity

Normally one atomic commit per implementation task.

## Commit message

```text
KOS-I2: implement <task description>
```

Examples:

```text
KOS-I2: implement runtime host
KOS-I2: implement component composition
KOS-I2: implement runtime configuration
KOS-I2: implement runtime observation
KOS-I2: implement failure handling
KOS-I2: add reference runtime demo
```

## Completion evidence

Record Build, Unit, Integration, Sanity, Regression, Review as PASS before commit. Record the resulting commit hash in task tracking.
