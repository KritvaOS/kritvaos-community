# FRL Requirements

See `docs/requirements/KOS-I4/REQUIREMENTS.md` for the normative traceability table.

Family: FRL

## Clarifications (planning review)

Exactly one ERROR event per endpoint fault transition (FRL-001); a link-level failure affecting N remote endpoints produces N endpoint ERROR events plus one link-level diagnostic record, which does not replace the endpoint events. No duplicate ERROR for an already-faulted endpoint. A request that has timed out can never be satisfied by a late response.
