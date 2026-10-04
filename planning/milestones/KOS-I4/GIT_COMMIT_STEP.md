# KOS-I4 Git Commit Step

For each task: inspect diff -> Debug/Release build -> unit/integration/sanity -> I2 regression -> I3 regression -> ASan -> UBSan -> mutation/fault injection -> review acceptance evidence -> one atomic commit.

Commit format: `KOS-I4 I4-00N <imperative description>`.

Record SHA in the task acceptance file and CHANGELOG. Do not squash task commits. Do not modify Core R1.0.
