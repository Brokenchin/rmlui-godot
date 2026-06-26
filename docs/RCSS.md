# RCSS Property Reference

RCSS is RmlUi's CSS dialect. This page lists every property **registered by
this build** (the same list `RmlManager.get_supported_rcss_properties()`
returns at runtime, and the one the editor autocomplete uses) plus the
differences from web CSS that matter day-to-day. Full semantics live in the
[upstream RCSS docs](https://mikke89.github.io/RmlUiDoc/pages/rcss.html).

## Differences from web CSS in 60 seconds

- **Units:** `dp` is the unit you want — device-independent pixels scaled by
  `RmlContext.dp_ratio`. `px` is raw pixels. `em`, `rem`, `%`, `vw`, `vh`
  exist. There is no `pt`.
- **Layout:** block/inline flow, **flexbox**, and tables are supported. There
  is **no CSS grid** — use flex.
- **Colors:** `#rgb` / `#rrggbb` / `#rrggbbaa`, `rgb()/rgba()`, named colors.
- **No `!important` cascade wars needed** — specificity rules match CSS.
- **Decorators replace `background-image`** (see below).
- **Selectors:** tag, `.class`, `#id`, `*`, descendant/child/adjacent
  combinators, attribute selectors, and structural pseudo-classes
  (`:hover`, `:active`, `:focus`, `:checked`, `:disabled`,
  `:first-child` family, `:nth-child` family) plus RmlUi's
  `:drag` / `:drag-over` for drag & drop visuals.
- `@media` queries (against context dimensions / dp-ratio), `@spritesheet`,
  `@keyframes`, `@decorator` at-rules are supported.

## Property index (registered in this build)

### Box model & layout
`display` `position` `top` `right` `bottom` `left` `inset` `float` `clear`
`z-index` `width` `height` `min-width` `min-height` `max-width` `max-height`
`box-sizing` `margin` `margin-top` `margin-right` `margin-bottom` `margin-left`
`padding` `padding-top` `padding-right` `padding-bottom` `padding-left`
`overflow` `overflow-x` `overflow-y` `clip` `visibility` `overscroll-behavior`

### Flexbox
`flex` `flex-basis` `flex-direction` `flex-flow` `flex-grow` `flex-shrink`
`flex-wrap` `align-content` `align-items` `align-self` `justify-content`
`gap` `row-gap` `column-gap`

### Borders & shape
`border` `border-width` `border-color` `border-top` `border-right`
`border-bottom` `border-left` `border-top-width` `border-right-width`
`border-bottom-width` `border-left-width` `border-top-color`
`border-right-color` `border-bottom-color` `border-left-color`
`border-radius` `border-top-left-radius` `border-top-right-radius`
`border-bottom-left-radius` `border-bottom-right-radius` `box-shadow`

### Text & fonts
`font` `font-family` `font-size` `font-style` `font-weight` `font-kerning`
`font-effect` `color` `text-align` `text-decoration` `text-transform`
`text-overflow` `letter-spacing` `line-height` `vertical-align` `white-space`
`word-break` `caret-color` `-rmlui-direction` `-rmlui-language`

Notes: `font-family` must name a **loaded face** (see `font_paths`) or a
generic mapped via `set_generic_family()`. `font-effect` applies glow /
outline / shadow / blur layers — these render through the Godot atlas
pipeline.

### Background, decorators & effects
`background` `background-color` `decorator` `image-color` `fill-image`
`filter` `backdrop-filter` `mask-image` `opacity`

`decorator` is the workhorse:

```css
.icon   { decorator: image("texture://minimap"); }          /* registered Godot texture */
.frame  { decorator: ninepatch("res://ui/frame.png", 12dp 12dp 12dp 12dp); }
.fade   { decorator: linear-gradient(90deg, #0008, #0000); }
.shader { decorator: shader(scanlines); }                    /* register_decorator_shader */
```

Built-in gradient decorators — `linear-gradient`, `radial-gradient`,
`conic-gradient` and their `repeating-*` variants — are rendered natively by the
bridge (procedural shader) and work everywhere, including the editor preview.

Custom `shader(...)` decorators, by contrast, are registered from GDScript
(`register_decorator_shader` / `register_decorator_material`), which doesn't run
in the editor — so in the **editor preview / 2D viewport** these elements render
without their shader (falling back to plain geometry). The missing shader is
reported once per name to the preview/diagnostics error bar, not per element, so
it never floods the editor.

`filter` / `backdrop-filter` accept `blur()`, `brightness()`, `contrast()`,
`drop-shadow()`, `grayscale()`, `hue-rotate()`, `invert()`, `opacity()`,
`saturate()`, `sepia()`.

### Animation
`transition` `animation` `transform` `transform-origin`
`transform-origin-x/y/z` `perspective` `perspective-origin`
`perspective-origin-x/y`

Transitions/animations cover most numeric and color properties; `@keyframes`
syntax matches CSS.

### Interaction & navigation (RmlUi-specific)
`cursor` `pointer-events` `drag` `focus` `tab-index`
`nav` `nav-up` `nav-down` `nav-left` `nav-right` `scrollbar-margin`

- `drag: none | drag | drag-drop | block | clone` — opts an element into the
  drag system (pair with `register_drag_source`).
- `tab-index: none | auto` — keyboard tab order.
- `nav-*` — explicit spatial-navigation wiring for gamepad/keyboard focus.
- Scrollbars are styled as real elements: `scrollbarvertical`,
  `scrollbarhorizontal`, with `slidertrack` / `sliderbar` /
  `sliderarrowinc` / `sliderarrowdec` children (see `base.rcss` for a
  template).

## Keeping this list honest

The editor autocomplete and this page both derive from
`RmlManager.get_supported_rcss_properties()` — if you update RmlUi and the
property set changes, regenerate the index with:

```gdscript
print(", ".join(RmlManager.get_supported_rcss_properties()))
```
