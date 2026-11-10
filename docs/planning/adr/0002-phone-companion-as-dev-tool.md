# 0002 — Phone companion becomes a development/test tool

- **Status:** Accepted
- **Date:** 2026-11-09
- **Supersedes:** the "phones as companion controllers" player-facing framing

## Context

1.x promotes phone-as-controller as the product's differentiator: an embedded
HTTP+WebSocket server, room codes, mDNS discovery and a PWA controller. A
top-down survival simulation does not need four party controllers, and keeping
the feature as a marketing pitch would force a shared-camera/input design that
constrains NPC density and UI interaction.

At the same time the infrastructure is genuinely useful: it is a second input
surface that connects over the network, from a different device, while the
simulation runs.

## Decision

Retire phone control as a **player-facing feature**. Keep `CompanionServer`,
`CompanionInput`, `RoomCode` and the PWA, and repurpose them as the owner's
**development and test client**: a browser or phone panel that can set the clock,
spawn and inspect NPCs, inject economy shocks, toggle debug overlays, teleport
the camera and drive input — including a virtual cursor for exercising the HUD
touch layout.

The dev client speaks the same command surface the automated tests use; it is a
view over test hooks, not a parallel code path.

## Consequences

- The companion protocol gains a **command channel** (dev-only, gated by a
  launch flag) alongside the existing input frame type; the versioned envelope
  absorbs it additively.
- The PWA is maintained as a dev tool: smaller feature surface, no onboarding
  flow to polish.
- No shared-camera or multi-cursor design work is needed.
- The promotion plan drops the "four phones" framing; the engine's story becomes
  simulation depth, and the phone becomes a developer-experience detail.

## Alternatives considered

1. **Keep phones as local co-op controllers** — real differentiator, but
   constrains the camera and UI and adds a second input model to every
   interaction.
2. **Delete the companion stack** — throws away working networking, mDNS, a PWA
   and its tests, and loses a genuinely convenient remote test surface.

## References

- `docs/planning/ROADMAP_TARGET.md` M2 (dev client), M3 (economy overlay)
