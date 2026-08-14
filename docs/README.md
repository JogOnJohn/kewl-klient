# Team documentation

This directory is the short, versioned handbook for KewlKlient contributors. It explains the current project shape and repeatable workflows; the code and `README.md` remain the ultimate detail.

| Document | Use it for |
| --- | --- |
| [Architecture](architecture.md) | Understanding how the launcher, native DLL, Java code, and plugins fit together. |
| [Development](development.md) | Preparing the VM checkout, building, validating, and committing changes. |
| [Reverse engineering](reverse-engineering.md) | Re-deriving offsets with Ghidra or IDA without mixing client builds. |
| [Research notes](research/README.md) | Recording reproducible, non-sensitive evidence. |

## Contribution standard

Every change should answer three questions:

1. **What changed?** Keep the diff focused and name the affected layer.
2. **How was it checked?** Record the command, observation, or static evidence.
3. **What remains uncertain?** Say `NOT VERIFIED` rather than promoting a guess to a fact.

That makes the project approachable for new contributors and prevents stale per-build findings from turning into mysterious crashes later.
