# KOS-I2 Test Plan

## Test levels

- Unit: isolated runtime logic.
- Component: component behavior with immediate dependencies.
- Integration: multi-component interaction.
- System: complete demo behavior.
- Sanity: build/launch/expected execution/clean exit.
- Regression: previously accepted functionality remains passing.

## Required order

```text
Build → Unit → Integration → Sanity → Regression → Review → Commit
```

## Entry criteria

Core R1.0 pinned; task scope approved; implementation builds.

## Exit criteria

All mandatory tests PASS; no unresolved blocker/critical regression; acceptance criteria and review checklist complete.

## Evidence

Record commands, results, test counts, failures, and final status in the task completion report/PR description.
