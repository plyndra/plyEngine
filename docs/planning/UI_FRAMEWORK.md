# UI framework spec

The HUD in the reference frames is roughly a third of what makes those screens
look finished: hotbar with selection, three gradient bars, an ornate minimap, a
clock/calendar, a 3-slice chat panel, and floating numbers over the world.

This is the spec for the screen-space layer. It is built **first**
([ADR 0007](adr/0007-hud-first.md)), against a real map loaded through the
native JSON format, so the HUD cannot be a pretty shell over nothing.

---

## 1. Coordinate model

Fixed by [ADR 0012](adr/0012-internal-resolution-and-tile-size.md): the world
renders at **640×360** internal and is upscaled by an integer factor (3× at
1080p, 4× at 1440p). UI lays out against that same **640×360 logical viewport**
and is drawn above the upscale.

- **World space**: 1 unit = 1 px at the internal render resolution. The world is
  rendered into a `RenderTarget` at that resolution, then upscaled by an integer
  factor.
- **Screen space**: 1 unit = 1 px at the *output* resolution, above the upscale.
  UI is not pixel-scaled by the world zoom; it scales by an explicit UI factor
  (1×, 2×, 3×) chosen by the user or by a "UI scale" setting.
- **Pixel snapping**: all UI rects are rounded to integers after layout, and text
  is drawn at integer sizes with nearest filtering. No half-pixel offsets
  anywhere — a 1 px border must be 1 px.
- **Logical size**: UI lays out against a logical viewport (e.g. 640×360) and the
  present step scales it by the integer factor, so a 4× window does not change
  layout.

## 2. Layout

```
Widget
  anchor:      TopLeft | TopCentre | TopRight | MidLeft | Centre | MidRight
             | BottomLeft | BottomCentre | BottomRight
  offset:      vec2 (from the anchored corner/edge, in logical px)
  size:        vec2 | (Fill, Fill) | (Fixed, Fill) ...
  pivot:       vec2 (where the widget's own origin sits, default {0,0})
  scale:       integer (1, 2, 3) — never fractional
  children:    laid out in a row, column or free placement
```

Rules:

1. **Anchor + offset, never absolute positions.** A 16:9 to 21:9 window change
   must not move the hotbar off centre.
2. **Integer scale only**, and the scale multiplies the source art *before*
   layout rounding.
3. **Layout is a pure function** of (logical size, widget tree, scale) →
   rect list. No animation state influences layout; animation only affects
   *drawn* content.
4. **Dirty layout**: recompute when the tree or logical size changes, else reuse
   the rect list. The cached rects are hashed in tests.

## 3. Nine-slice and three-slice

| Mode | Corners | Edges | Centre |
|---|---|---|---|
| `NineSlice` | fixed | stretch or tile | stretch or tile |
| `ThreeSliceH` / `ThreeSliceV` | end caps fixed | — | stretch or tile |
| `Plain` | — | — | stretch |

**Margins** come from the atlas entry: a `margins` array of
`[left, top, right, bottom]` in source pixels, pre-scale, with `margins_source`
recording where they came from (`"declared"` or `"derived"`) — §4 is the
normative field shape. When an entry declares none — generated art usually does
not — margins are **derived from the alpha contour**: find the run-length of
uniform border pixels per side and use that as the margin, then verify the
corners are stretch-invariant. This mirrors what `slicewright` does for
generated UI, and it is why provenance is a separate field rather than an
overloaded `margins`.

**A source region must keep a centre**: an individual margin larger than half
that axis, or a pair summing to the whole axis, is rejected at load with a named
error — there would be nothing left to slice, or a zero-width centre that cannot
tile.

**Degenerate sizes**: if a target rect is smaller than corner sum + 1, the caps
compress uniformly (documented behaviour, not clipped garbage).

**Tiled fill**: a tiled edge or centre emits whole tiles from the region's
start corner and clips the final tile at the target edge, so tile boundaries stay
pixel-exact. The clipped remainder is part of the structural hash, not an
emergent rounding artefact. (Decided 2026-11-10, before a2's geometry existed.)

## 4. Atlas schema

Follows the shape of the CC0 Hearthmark pack's `atlas.json`, which is a good
precedent for what a UI atlas needs:

```json
{
  "image": "ui.png",
  "palette": "ui-32",
  "regions": {
    "panel.wood":  { "x": 0,  "y": 0, "w": 48, "h": 48,
                     "margins": [12, 12, 12, 12],
                     "margins_source": "declared",
                     "stretch": { "edges": "tile", "centre": "stretch" } },
    "bar.frame":   { "x": 96, "y": 0, "w": 64, "h": 16,
                     "margins": [4, 4, 4, 4], "margins_source": "derived" },
    "slot.normal": { "x": 160, "y": 0, "w": 20, "h": 20 },
    "cursor.point":{ "x": 200, "y": 0, "w": 8,  "h": 8, "hotspot": [1, 1] },
    "panel.tip":   { "x": 0, "y": 96, "w": 32, "h": 24,
                     "margins": [8, 6, 8, 6], "margins_source": "declared",
                     "slice": "three_h" }
  },
  "animations": {
    "next-arrow": { "frames": ["arrow.0", "arrow.1", "arrow.2"], "duration_ms": [120, 120, 200] }
  },
  "fonts": { "ui": { "file": "ui.ttf", "sizes": [10, 20, 30] } }
}
```

`slice` is `nine` (the default), `three_h`, `three_v` or `plain`; `plain`
stretches the whole region and ignores margins. A region that omits both margin
keys has no margins and behaves as `plain` until a3 derives them.

The loader ignores keys it does not know, so a newer document still loads in an
older build. `emit` writes the canonical form — region, animation and font
entries in name order, default-valued fields omitted, two-space indent — which is
what makes load→emit→load byte-identical rather than merely equal.

`margins_source` matters: `derived` entries are re-derived and re-checked at
load and fail the art lint if the derived values drift outside ±1 px of the
recorded ones.

## 5. Widgets (M1a scope)

| Widget | Behaviour | Test |
|---|---|---|
| `Panel` | nine-slice background + optional title bar | corner invariance at 3 sizes |
| `Bar` | frame + gradient fill + segment ticks; value 0..1 | fill fraction → pixel width; clamps at 0/1 |
| `Slot` | nine-slice frame + icon + stack count + selection state | state → region selection; count formatting |
| `Hotbar` | row of slots, selection index, keyboard/phone input | selection wraps; click hits the right slot after layout rounding |
| `InventoryGrid` | row/column layout over a container model | item at index maps to the right cell |
| `Tooltip` | follows cursor, clamps inside the logical viewport | never leaves bounds for any cursor position |
| `Minimap` | render-target downscale + marker transforms | world (x,y) → minimap px, verified for 4 corner cases |
| `DialogueBox` | 9-slice box, name plate, portrait frame, typewriter reveal | reveal state; skip advances |
| `FloatingText` | world→screen projection, rise + fade, integer positions | projection matches camera; fades deterministically |

Deferred: focus/scroll containers, drag-and-drop, tab bars, text wrapping beyond
simple word wrap.

## 6. Input routing

```
raw input (keyboard/mouse/dev client)
  → UIRouter: does the cursor hit a widget with an interactive rect?
      yes → widget queue (focus, hover, press, release)
      no  → world queue (move, aim, interact)
  → both: a global handler (pause, toggle debug)
```

Rules: UI consumes before world; a widget "captures" on press and keeps the
release even if the cursor leaves; the phone dev client's virtual cursor uses the
same router, so a HUD button can be pressed from a phone (useful for testing
touch layouts without a touch device).

## 7. Text

Pixel font at integer sizes only (10/20/30 px or similar), nearest filtering, no
AA, no fractional scaling, baseline alignment by ascent. Two sources: a BMFont
`.fnt` (fast, exact) and a TTF rasterized at load into an atlas for the sizes
declared in the JSON. Missing glyphs fail the art lint rather than rendering as
blanks.

## 8. Verification

| Invariant | How |
|---|---|
| Layout purity | layout(tree, size, scale) hashes identically across runs and processes |
| Corner invariance | scale a panel 3× in each axis; the 4 corner regions hash identically |
| Integer snapping | every drawn rect has integral x/y/w/h after layout |
| No bleed | a slot's icon never samples outside its region (checked with a 1 px border fixture) |
| Anchor correctness | 9 anchors × 3 logical sizes × 3 scales → expected rects |
| Text bounds | measured bounds == drawn quads' extents |
| Determinism | whole HUD state (rects, fill fractions, animation phase) hashes from (clock, model) |

Golden-frame hashes are committed for the HUD composited over the fixture map, so
a regression in the UI layer shows up as a hash diff, not a screenshot review.

## 9. Non-goals (M1)

Retained-mode binding/refresh frameworks, styling/theme inheritance beyond a flat
palette, animation curves beyond linear + ease-in/out, localisation plumbing,
accessibility scaling beyond the UI-scale setting.
