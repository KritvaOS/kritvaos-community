# KritvaOS Agent Implementation Rules

1. Follow `docs/architecture/` and `docs/development/REPOSITORY_STRUCTURE.md`.
2. For KOS-I2, treat `kritva-core` R1.0 as a released dependency; do not modify `core/` for normal I2 work.
3. Do not duplicate Core contracts or introduce speculative I3/I4 abstractions.
4. Implement only approved task scope; no unrelated refactors.
5. Every task requires unit, integration where applicable, sanity, and regression verification.
6. Do not declare completion until acceptance criteria and review checklist are satisfied.
7. Record test evidence and commit hash.
8. Stop for architectural review if a Core API change, requirement ambiguity, or scope expansion is required.
9. Do not commit until tests, sanity, regression, diff review, and acceptance gates pass.
