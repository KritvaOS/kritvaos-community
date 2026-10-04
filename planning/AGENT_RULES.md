# KritvaOS Agent Implementation Rules

1. Follow `docs/architecture/` and `docs/development/REPOSITORY_STRUCTURE.md`.
2. Treat `kritva-core` R1.0 as a released dependency; do not modify `core/` for normal I3 work.
3. Implement only the approved milestone/task scope.
4. Do not duplicate Core contracts.
5. Device/Endpoint shall not become a competing Core lifecycle engine.
6. Keep I3 transport-neutral; do not introduce I4 Nexus/Edge/EtherCAT abstractions.
7. Every task requires unit, integration where applicable, sanity, and regression verification.
8. Do not declare completion until acceptance criteria and review checklist are satisfied.
9. Record commands, test evidence, and commit hash.
10. Stop for architectural review if a Core API change, requirement ambiguity, or scope expansion is required.
11. Do not commit until tests, sanity, regression, diff review, and acceptance gates pass.
