# Research notes

Keep one short Markdown note per meaningful reverse-engineering finding or feature investigation. Notes belong here only when they contain reproducible, non-sensitive evidence. Never place account data, session identifiers, private keys, copied executables, or databases in this directory.

Use this template:

```markdown
# <short finding title>

- Date: YYYY-MM-DD
- Client SHA-256: `<exact hash>`
- Scope: function RVA | struct field | API behaviour
- Status: DERIVED | OBSERVED | RUNTIME CONFIRMED | NOT VERIFIED

## Anchor and path

- Client-owned anchor:
- Registration location:
- Leaf location or data path:

## Finding

Describe the derived RVA, field chain, or behaviour in words. Link the source file and line that would consume it, if applicable.

## Evidence and next step

State exactly what was inspected and what would raise confidence.
```

If the finding changes `client/offsets.hpp`, update `BUILD_ID` in the same commit and keep the note's client hash aligned with that commit.
