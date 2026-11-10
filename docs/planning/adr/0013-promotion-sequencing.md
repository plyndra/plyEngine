# 0013 — Promotion: publish the write-up, hold the one-shot channels

- **Status:** Accepted
- **Date:** 2026-11-09
- **Amends:** `docs/PROMOTION_PLAN.md` phases 1–2

## Context

The promotion plan was written when the platformer demo was the landing page and
assumed Show HN in the week of 2026-12-01. [ADR 0001](0001-top-down-sim-identity.md)
replaces that demo, and the 2.0 village frame is weeks away. Two facts pull in
opposite directions:

- The incident write-up ("what broke only after people could download it") is
  **about 1.0** and stays true no matter what the demo becomes. It is durable and
  already drafted and fact-checked.
- Show HN and curated-list PRs are **single-attempt** surfaces. Sending that
  traffic to a landing page that is mid-rewrite spends the one introduction on a
  half-built first impression.

## Decision

1. **Publish the write-up** (Phase 1.1) on its current schedule — GitHub
   Discussions as canonical, `docs/announcements/` as source of truth.
2. **Hold Show HN, Discord `#showcase` and any curated-list PR** until the 2.0
   village frame exists and the README's hero asset shows it.
3. Phase 0 metadata work already shipped (description, topics, homepage,
   Discussions) stands; it improves the landing page under either demo.
4. The 1.0.1 release remains the artefact the write-up links to. Its demo-exit
   fix is part of the story, not separate from it.
5. The hero asset, "try it in 30 seconds" block and social card are **rebuilt**
   against the new demo at M1c.

## Consequences

- The one-shot doors stay unspent, which is the whole point of the plan's
  "one attempt per door" principle.
- Promotion effort between now and M1c is limited to the write-up and 1.4-style
  replies — no channel launches, no vote solicitation, no cross-posting.
- The 2026-12-01 floor becomes a **superseded date**; the Show HN trigger returns
  to the readiness gate already written into the plan (0.5 live, Phase 1 gate met,
  owner free for 4–6 hours).
- The measured 0.46 s demo death becomes the write-up's lead incident (already
  done), so the material ages better than a feature announcement would.
- The repo will look "quiet" for the duration. Accepted: the alternative is
  spending attention on a page that is about to change.

## Alternatives considered

1. **Run the full plan against 1.0.1 now** — fastest to attention, but every post
   lands on a page that changes underneath it, and HN cannot be re-run.
2. **Freeze all promotion until M1c** — idles the metadata work already shipped
   and lets the incident material age for months.
3. **Post the write-up *and* HN now, accept the demo swap later** — the failure
   mode this ADR exists to prevent.

## References

- `docs/PROMOTION_PLAN.md` (phases 1–2, gates)
- `evals/promotion.md` C0.5, C0.6, C2
- `docs/announcements/2026-11-09-discord-announcement.md`
