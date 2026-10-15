# Architecture Decision Records

Short documents that capture an important technical decision: the context, what was decided, and
its consequences. Accepted ADRs are not edited. A changed decision gets a new ADR that marks the
old one as *Superseded*.

| ADR | Title | Status |
|-----|-------|--------|
| [0001](0001-sdl3-gpu-api.md) | Use SDL3 and the SDL3 GPU API instead of OpenGL | Accepted |
| [0002](0002-zero-asset-policy.md) | Zero external asset files | Superseded by 0004 |
| [0003](0003-headless-core.md) | Headless core with optional modules | Accepted |
| [0004](0004-procedural-first-assets-optional.md) | Procedural‑first, assets optional | Accepted |

## Template

```markdown
# NNNN. Title

- Status: Proposed | Accepted | Superseded by NNNN
- Date: YYYY-MM-DD
- Issues: #N

## Context
What forces are at play? What problem are we solving?

## Decision
What we will do.

## Alternatives considered
Each option and why it was not chosen.

## Consequences
What becomes easier, what becomes harder, follow-up work.
```
