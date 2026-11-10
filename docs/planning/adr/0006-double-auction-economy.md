# 0006 — Economy: agent-to-agent bargaining via a batched double auction

- **Status:** Accepted
- **Date:** 2026-11-09

## Context

The target includes an in-game smart economy. The richest model is pairwise
agent-to-agent bargaining with inventories and bids; it is also an order of
magnitude more expensive than every other system in the simulation combined, and
much harder to keep deterministic and debuggable.

## Decision

Ship **agent-to-agent bargaining as a batched double auction** (per-good order
books, cleared in ticks, agents submitting bids/asks from their own inventories,
money and needs) behind a `MarketPolicy` interface. Pairwise negotiation can
replace or augment it later without an API break.

## Consequences

- Emergent price discovery, shortage propagation and arbitrage all appear, at
  ~1/10 the tick cost of pairwise negotiation.
- Determinism is tractable: clearing order is defined by (price, submission seq,
  agent id), so two runs agree byte-for-byte.
- CI invariants: money conserved per batch; no item created or destroyed outside
  declared production; prices finite and positive across a 30-day soak.
- "Bargaining" is a market mechanism, not one-on-one haggling. If dialogue-level
  haggling is wanted later, it becomes a policy on top, not a rewrite.

## Alternatives considered

1. **Pairwise bargaining now** — best narrative potential, worst cost and
   determinism burden.
2. **Abstract price curves** — cheapest, but the economy becomes scenery: player
   supply decisions stop mattering.
3. **Double auction, no order book (scalar price per good)** — simplest, but
   loses who-traded-with-whom, which the UI and the tests both want.

## References

- `docs/planning/ARCHITECTURE_TARGET.md` §8
- `docs/planning/ROADMAP_TARGET.md` M3
