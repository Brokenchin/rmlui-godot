# API Reference

GDScript-facing API of the rmlui-godot plugin. Three classes:

- [`RmlContext`](#rmlcontext) — the Control node that hosts and renders documents
- [`RmlManager`](#rmlmanager) — Engine singleton: global fonts, textures, defaults, logging
- [`RmlElementHandle`](#rmlelementhandle) — RefCounted wrapper around a live document element

> **Thread safety:** every method here must be called from the **main thread**.
> RmlUi has no internal locking and binding callbacks fire during the context's
> `_process` update. Off-thread calls log a one-time warning — marshal through
> `call_deferred()` instead.

---

## RmlContext

`RmlContext` extends `Control`. Drop it in a scene, set `document_path`, done.
Each instance owns an isolated `Rml::Context`; fonts and textures are shared
process-wide (see RmlManager).

### Properties (inspector)

| Property | Type | Notes |
|---|---|---|
| `rml_context_name` | String | RmlUi context name. Duplicates get a unique suffix automatically. |
| `dp_ratio` | float (0.25–4.0) | Density-independent pixel ratio — scales all `dp` units. Live. |
| `document_path` | String (`*.rml`) | Auto-loads in `_ready` **and renders in the editor**. Changing it live swaps the document. |
| `font_paths` | PackedStringArray | Faces loaded before the document. Repeated loads of the same file are deduplicated process-wide. |
| `use_default_rcss` | bool | Apply the built-in (or overridden) base stylesheet to every document. |
| `text_render_mode` | enum | Default / SubPixel Offset / Godot Native / RmlUI Native. See README §Text Rendering. |
| `font_pixel_snap`, `font_layout_mode`, `font_hinting`, `font_antialiasing`, `font_subpixel`, `font_oversampling` | — | Granular text tuning; apply process-wide via the shared font engine. |
| `gpu_scissor` | bool | Shader-based scissoring instead of CPU clipping. |
| `text_filtering_mode` | enum (Nearest/Linear) | Texture filter for **text glyphs only** — default **Nearest** (crisp, like Godot's own text), independent of the project default and the node's `texture_filter`. Images/decorators keep normal project filtering. Linear is available for smooth-scaled UI; glyph sampling under linear is softness-prone until the SUBPIX_OFFSET alignment follow-up lands. |
| `editor_mock_data` | Dictionary | `{model_name: {var_name: value}}` — mock models stood up **in the editor only** (preview panel and 2D viewport) so `data-for`/`{{ }}` render without running the game. Arrays become data arrays — arrays of dictionaries drive struct-array `data-for` rows for preview. |
| `input_actions` | PackedStringArray | InputMap actions this context watches via `_unhandled_input`. Each press/release emits `rml_input_action` **and** dispatches to the documents' `<script>` blocks (`_on_input_action(action, pressed)`). Empty = zero input overhead. |
| `debugger_toggle_key` | int (godot Key) | Key toggling the RmlUi debugger overlay. Default `KEY_F10`, `0` disables. (F8/F9 collide with the editor's Stop/Pause shortcuts for the embedded game window.) Also callable directly: `toggle_debugger()`. |
| `gamepad_navigation` | bool | Routes Godot's `ui_up/down/left/right`, `ui_focus_next/prev`, `ui_accept`, `ui_cancel` into RmlUi's built-in focus engine: spatial navigation over elements with `nav: auto` (or explicit `nav-*`), tab order via `tab-index: auto`, accept = click on the focused element. Works for keyboard *and* gamepad (the default `ui_*` bindings include D-pad/sticks). A consumed press is marked handled so gameplay doesn't double-react. First directional press auto-focuses the first tabbable element. |

Configuration warnings (⚠ in the scene tree) flag a missing `document_path`,
nonexistent document/font files, and no-fonts-anywhere setups.

### Signals

| Signal | Args | When |
|---|---|---|
| `rml_drag_started` | `element_id: String, payload: Dictionary` | A registered drag source started dragging. |
| `rml_drag_entered` | `element_id: String, data: Dictionary` | Mid-drag, the cursor entered a registered drop target. `data` = the drag payload. |
| `rml_drag_over` | `element_id: String, data: Dictionary` | Mid-drag, the cursor moved while over a registered drop target. |
| `rml_drag_left` | `element_id: String` | Mid-drag, the cursor left a registered drop target (also fires when the drag ends — after `rml_drop_received` on a drop). |
| `rml_drop_received` | `element_id: String, data: Dictionary` | A registered drop target received a drop. |
| `rml_element_hovered` | `element_id: String, global_position: Vector2` | The cursor entered an element carrying an id (nearest id ancestor). |
| `rml_element_unhovered` | `element_id: String` | The cursor left the previously-hovered element. |
| `rml_input_action` | `action: String, pressed: bool` | A watched InputMap action (see `input_actions`) was pressed/released. |

### Documents

```gdscript
load_document(path: String)
load_document_from_string(rml_text: String, alias_path := "memory://document") -> bool
reload_document(path: String) -> bool
reload_all_documents()
unload_document(path: String) -> bool
get_loaded_documents() -> Array          # paths/aliases
get_document_script(document_path := "") -> Variant    # first <script> block instance
get_document_scripts(document_path := "") -> Array     # all block instances, document order
```

- `load_document_from_string` parses from memory; `alias_path` is used for
  error messages **and** to resolve relative `<link href>` — pass the real
  path when the text mirrors a file.
- `reload_document` preserves data models and re-dirties all variables.
- `get_document_script("")` returns the first `<script>` block instance of any
  loaded document (forcing its lazy instantiation), or `null`. Connect to its
  signals or call its methods from game code.

### Embedded sub-documents

Embed another authored document (its own `.rml` + `<link>` RCSS + `<script>`) as
a **real subtree** of this context's DOM, so it shares the parent's layout domain
— flexbox, `@media`, anchoring, `overflow` all reach across into it and sibling
widgets reflow around it — while keeping its own GDScript `<script>` instance.
This is the composition primitive: one **coordinator** context mounts N
separately-authored panels and owns their shared layout (so e.g. an enlarging
character screen pushes the inventory bag aside via CSS instead of overlapping
it). It is *not* render-to-texture — the embed is a first-class layout participant.

```gdscript
mount_embed(parent_element_id: String, src: String, options := {}) -> String   # returns embed_id ("" on failure)
unmount_embed(embed_id: String) -> bool
reload_embed(embed_id: String) -> bool
get_embedded_script(embed_id: String) -> Variant     # first <script> instance of the embed
get_embedded_scripts(embed_id: String) -> Array      # all block instances, document order
get_embedded_element(embed_id: String, inner_id: String) -> RmlElementHandle   # scoped to one embed
get_embedded_data(embed_id: String, model_name := "") -> RmlDataModel           # the embed's (namespaced) model
get_data_model_handle(model_name: String) -> RmlDataModel                       # any model, same handle type
get_embedded_ids() -> PackedStringArray
is_embed_mounted(embed_id: String) -> bool
```

`options`: `{ "id": String, "model": String }` — `id` names the embed (else one is
auto-generated and returned); `model` records a data-namespace hint for the embed.

The embedded document is mounted as the child of an `<embed-doc>` host element,
which becomes the flex item / anchored box in the parent's layout; the document
fills it. The host carries the embed id, so `get_element_by_id(embed_id)` returns
its handle.

**Declarative form** — author the host directly in the parent `.rml`; it is
mounted automatically on document load (and re-mounted on `reload_document`):

```html
<body>
  <div id="hud-top"> … </div>
  <embed-doc id="bag" src="bag.rml" model="bag"/>
  <embed-doc id="char" src="character.rml" model="char"/>
</body>
```

A relative `src` resolves against the parent document's directory; `res://` /
`user://` / absolute paths are used as-is. Embeds may themselves contain
`<embed-doc>` (depth- and cycle-guarded).

**Parent → child data feed (recommended: the cached script instance).** The
embedded `<script>` runs as its own GDScript instance with `var rml_context`
injected (the coordinator node) — that instance *is* the panel's view-model.
Fetch it once with `get_embedded_script`, cache it, and drive the panel through
its own typed methods (no stringly-typed data calls). Give the embed `<script>` a
`class_name` and you get full type-checking and autocomplete:

```gdscript
# character_panel.gd  (the embed's <script src> — or an inline block)
class_name CharacterPanel
var rml_context
func set_vitals(hp: int, mp: int) -> void: ...
func add_xp(amount: int) -> void: ...
```
```gdscript
# coordinator
var _char: CharacterPanel
func _on_hud_loaded() -> void:
    _char = $Hud.get_embedded_script("char") as CharacterPanel   # cache the class
func _on_damage(dmg: int) -> void:
    _char.set_vitals(player.hp, player.mp)                        # typed call, no strings
```

This is the explicit, collision-free path and the preferred one for complex UI.
For declarative list rendering inside an embed (`data-for` / `{{ }}`), use the
data-binding API with a model name unique to that embed (see the namespacing note
below).

To reach a specific element inside an embed (e.g. to set a class or read state),
use **`get_embedded_element(embed_id, inner_id)`** — it is scoped to that one
embed, so two embeds of the same `.rml` with identical internal ids stay
addressable independently:

```gdscript
$Hud.get_embedded_element("char", "portrait").set_class("enraged", true)
```

**Declarative data (`data-for` / `{{ }}`) per embed — `RmlDataModel`.** For list
rendering inside an embed, opt the embed into per-instance data isolation with the
**`model`** attribute (`<embed-doc model="..."/>` or `options.model`). Its
`data-model="x"` then binds to a model unique to that embed, so two instances of
the same `.rml` never collide. Drive it with a cached handle:

```gdscript
var grid := $Hud.get_embedded_data("inventory")   # RmlDataModel for this embed's model
grid.set_array("cells", cells)                     # feeds data-for; bind-on-first-use then update
grid.push("cells", {icon = "...", qty = 2})
grid.set_value("title", "Backpack")
grid.bind_event("on_cell", _on_cell)               # data-event-click="on_cell"
```

The same handle is injected into the embed's `<script>` as **`var data`** (like
`var rml_context`), so a reusable component can own its rendering and expose typed
methods — the parent stays decoupled from the component's internal variable names:

```gdscript
# grid_panel.rml <script>  — drives its own (namespaced) model
var data
func add_cell(c): data.push("cells", c)
```

`RmlDataModel` methods: `set_value(key, v)` / `get_value(key)` / `update(dict)`,
`set_array(name, arr)` / `push(name, v)` / `remove_at(name, i)` / `set_item(name, i, v)` /
`array_size(name)` / `clear_array(name)`, `bind_event(name, callable)`, `dirty(key)` /
`dirty_all()`, `is_valid()`. `get_data_model_handle(name)` returns one for *any*
model (root document or embed) — the same ergonomic handle everywhere.

Notes / current limits:
- Inline `gdscript:` handlers and `<script>` blocks inside the embed resolve to
  the embedded document — a button inside an embed runs the embed's own handler,
  and a signal it emits is reachable via `get_embedded_script` (RmlUi preserves a
  document's owner across mounting).
- The embed's RCSS theme cascades in from the base stylesheet; its own `<link>`
  styles stay local to its subtree.
- `get_element_by_id` is context-global (first match wins across embeds) — use
  `get_embedded_element(embed_id, inner_id)` for unambiguous per-embed access.
- Imperatively-mounted embeds are **not** restored by `reload_document` (re-mount
  them from game code); declarative `<embed-doc>` embeds are.
- Per-embed data-model namespacing is **opt-in** via `model` — without it, an
  embed binds to context-global models by their authored names (simplest for a
  single instance). The namespace rewrite happens once at mount, not per frame.
- **Input** crosses the boundary: mouse hit-testing (`get_element_at_point`),
  clicks, hover, and keyboard/gamepad focus navigation all reach embedded
  elements. **RCSS is document-scoped** (an embed's styles don't leak to the
  parent or vice versa). **`@media`** re-evaluates for both the parent and the
  embed on context resize.

### Fonts

```gdscript
load_font_face(path: String) -> bool
load_font_face_ex(path: String, family: String, style := 0, weight := 400, fallback := false) -> bool
load_font_resource(font: Font) -> bool
load_font_resource_ex(font: Font, family := "", weight := 0, fallback := false) -> bool
set_generic_family(generic_name: String, family_name: String)   # e.g. "sans-serif" → "Noto Sans"
get_generic_family(generic_name: String) -> String
```

Faces are **global** (one font engine per process) and live until shutdown.
File-based loads deduplicate on `(path, family, style, weight, fallback)` —
loading the same font from many contexts costs one copy.

### Data binding

```gdscript
create_data_model(model_name: String) -> bool
create_data_model_from_dict(model_name: String, variables: Dictionary) -> bool
bind_data_variable(model_name, variable_name, initial_value) -> bool
set_data_variable(model_name, variable_name, value)      # auto-dirties
get_data_variable(model_name, variable_name) -> Variant
update_data_model(model_name, variables: Dictionary)     # batch set + dirty
bind_data_event(model_name, event_name, callable) -> bool  # data-event-* → Callable(args: Array)
dirty_data_variable(model_name, variable_name)
dirty_all_variables(model_name)

bind_data_array(model_name, array_name, initial_array) -> bool
set_data_array(model_name, array_name, array)
push_data_array_item(model_name, array_name, value)
set_data_array_item(model_name, array_name, index, value)
remove_data_array_item(model_name, array_name, index)
get_data_array_size(model_name, array_name) -> int
clear_data_array(model_name, array_name)
```

Models must exist **before** the document that references them loads
(`data-model="name"`), or the document logs "Could not locate data model".

Arrays may hold scalars **or dictionaries**. A dictionary element becomes a
struct whose keys are accessible as members inside the repeated element
(`data-for="slot : slots"` → `slot.icon`, `slot.count > 1`, …); field types are
preserved, and dictionaries/arrays nest arbitrarily. All array mutators
(`set_data_array`, `push_data_array_item`, `set_data_array_item`, …) accept the
same scalar-or-dictionary values. All mutators auto-dirty.

### Elements & events

```gdscript
get_element_by_id(id: String) -> RmlElementHandle    # searches all loaded documents
get_element_at_point(point: Vector2) -> RmlElementHandle   # youngest element at a point (crosses into embeds)
get_focused_element() -> RmlElementHandle                  # element with input focus, or invalid
add_event_listener(element_id, event_type, callable, in_capture_phase := false) -> bool
remove_event_listeners(element_id, event_type) -> int
set_element_property(element_id, property, value) -> bool
remove_element_property(element_id, property)
set_element_class(element_id, class_name, activate)
set_element_inner_rml(element_id, rml)
get_element_outer_rml(element_id) -> String
get_element_attribute(element_id, attribute, default_value := "") -> String
set_element_attribute(element_id, attribute, value)
register_custom_element(tag_name, on_create, on_attribute_change := Callable()) -> bool
```

Event callables receive one Dictionary: `type`, `target_id`, `phase`,
mouse position, parameters. Handles go stale when their document unloads —
check `is_valid()` after reloads.

### Drag & drop

```gdscript
register_drag_source(element_id, payload_builder, ghost_builder := Callable())
register_drop_target(element_id, drop_handler := Callable())
```

`payload_builder(element_id, position) -> Dictionary` supplies the drag data;
optional `ghost_builder` returns custom ghost RML. Bridges Godot's native drag
system — works across RmlContexts *and* native Controls. Callables from inline
`<script>` blocks work (`rml_context.register_drag_source(...)`).

The drag ghost renders on its **own dedicated `CanvasLayer`** (following the
cursor, freed on drop) rather than via Godot's source-relative
`set_drag_preview`, so it always draws above the rest of the UI regardless of
which widget the drag started from. The layer index is configurable globally:

```gdscript
RmlManager.drag_ghost_layer = 128          # or RmlManager.set_drag_ghost_layer(128)
```

It defaults to `128` (the top `CanvasLayer`) and is seeded from the
`rmlui/drag/ghost_layer` project setting; values are clamped to the
`CanvasLayer` range `[-128, 128]`. Lower it to slot the ghost into a custom
layer scheme (e.g. above panels but below a modal). Native drop detection
(`register_drop_target`) is unaffected.

#### Drag-target events (issue #39)

```gdscript
get_drag_over_target() -> String   # target the active drag is over, or ""
# signal rml_drag_entered(element_id: String, data: Dictionary)
# signal rml_drag_over(element_id: String, data: Dictionary)
# signal rml_drag_left(element_id: String)
```

The drag-time counterpart of the hover bridge: while a drag is in progress,
entering/leaving a **registered drop target** fires `rml_drag_entered` /
`rml_drag_left`, and `rml_drag_over` fires on cursor movement while over the
target (never when the cursor is still). `data` is the same payload the drop
handler receives (`gui_get_drag_data()`), so the game can highlight only the
slots that accept the dragged item, or pop a live comparison card:

```gdscript
ctx.rml_drag_entered.connect(func(id, data):
	if data.get("slot_type") == slot_types[id]:
		ctx.set_element_class(id, "drag-target", true))
ctx.rml_drag_left.connect(func(id):
	ctx.set_element_class(id, "drag-target", false))
```

Events fire for any native drag over this context — including drags started
from other Controls or contexts. On a drop the order is: drop handler +
`rml_drop_received`, then the final `rml_drag_left` (highlight cleanup); on a
canceled drag only `rml_drag_left` fires. The existing drop handler and the
ghost are unaffected.

### Hover bridge

```gdscript
get_hovered_element_id() -> String   # nearest id under the cursor, or ""
# signal rml_element_hovered(element_id: String, global_position: Vector2)
# signal rml_element_unhovered(element_id: String)
```

Mirrors the drag bridge for tooltips that must escape the source document's
clipping. Connect `rml_element_hovered` / `rml_element_unhovered` and render the
tooltip in a **separate, screen-sized overlay context** so it isn't clipped by
ancestor `overflow` or this context's viewport. Resolution is by element id at
event time (the nearest ancestor carrying an `id`), so it works for slots
streamed in via `set_element_inner_rml` — the case where data-binding attributes
don't reliably bind. `global_position` is the cursor's position when the hover
began. The signal fires once on enter and once on leave, not on motion within
the element; poll `get_hovered_element_id()` on mouse motion if you need a
following tooltip. Leaving the context entirely (cursor exits the Control) also
emits `rml_element_unhovered`.

### Input pre-handler (game-first routing)

```gdscript
set_input_prehandler(handler := Callable())   # handler(event: InputEvent) -> bool
set_input_tick(handler := Callable())         # handler(delta: float) -> void
```

The high-level bridges above (`on*` events, `register_drag`, the hover bridge)
are the RmlUi-*first* path. The pre-handler is the low-level game-*first* one:
`handler` runs on every `_gui_input` event **before** RmlUi (and the native
drag) sees it. Return `true` to **consume** the event — RmlUi and the native
drag both skip it; return `false` (or nothing) to **forward** it, leaving click
/ hover / drag / `on*` exactly as they are today. Mirrors Godot's `_input`
chain. Pass an empty `Callable()` to clear either handler.

Inside the handler you can call `get_hovered_element_id()` and read the in-flight
drag payload to know *what* you're acting on, then route to bound InputMap
actions however your game wants (rebinding, controller gestures) — all in
GDScript, not the addon.

`set_input_tick` delivers a per-frame callback so **time-based** gestures work
without a node. An event hook alone can't fire long-press (it triggers on
elapsed time with no further event); the tick lets a document `<script>` detect
"held 400 ms in place" itself. With *events + tick*, long-press, double-tap
timeout, hold-to-charge, and chords are all pure game code — zero addon work per
gesture. The tick runs each frame after the context update (so it sees a settled
layout and hover chain).

If the pre-handler consumes the mouse press that would have begun a native drag,
the drag is suppressed for that gesture, so a long-press timer started on a slot
doesn't fight `register_drag_source`.

```gdscript
# Long-press → context selector, no onlongpress addon feature needed:
var _hold_id := ""
var _hold_t := 0.0
func _ready():
    rml_context.set_input_prehandler(_route_input)
    rml_context.set_input_tick(_tick)

func _route_input(event: InputEvent) -> bool:
    if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
        if event.pressed:
            _hold_id = rml_context.get_hovered_element_id()
            _hold_t = 0.0
        else:
            _hold_id = ""                       # released early — let RmlUi click
    return false                                # forward; we only time the hold

func _tick(delta: float) -> void:
    if _hold_id == "":
        return
    _hold_t += delta
    if _hold_t >= 0.4:
        open_selector(_hold_id)
        _hold_id = ""
```
### Mouse input & hit-testing

An `RmlContext` is a `Control` and keeps `mouse_filter = STOP` so RmlUi receives
clicks, but it does **not** consume mouse events over its whole rect. It overrides
`_has_point()` to hit-test the live DOM: Godot's mouse picking only "sees" the
context where an RML element actually sits under the cursor, so empty / transparent
gaps fall through to controls (and lower `CanvasLayer`s) behind it. This makes a
fullscreen or sparse context (a centered panel, a HUD with gaps, scattered widgets)
non-blocking where there's no content.

- A bare document **body** counts as a hit only when it paints something — an
  opaque `background-color` or a `decorator`. A transparent fullscreen body passes
  through; give it a background (or wrap content in a sized element) to capture
  clicks on its empty area.
- Opt a specific element out of hit-testing with RCSS `pointer-events: none` —
  it (and its bare areas) then pass through to whatever is below.

### Styling & rendering

```gdscript
inject_stylesheet(rcss_string: String) -> bool   # combined ON TOP of each loaded document's styles
set_base_rcss(rcss) / get_base_rcss() / append_base_rcss(rcss) / reset_base_rcss()
register_texture(name, texture: Texture2D) -> bool      # per-context "texture://name"
unregister_texture(name) -> bool
register_decorator_shader(name, shader: Shader) -> bool  # decorator: shader(name)
register_decorator_material(name, material: ShaderMaterial) -> bool
unregister_decorator_shader(name) -> bool
get_context_info() -> Dictionary   # initialized, name, size, docs, models, listeners, geometry, textures…
```

`inject_stylesheet` appends at the end of the cascade — it overrides document
styles and is not removable individually (reload the document to reset).
`base_rcss` replaces the built-in default sheet for this context only.

---

## RmlManager

Engine singleton (`Engine.get_singleton("RmlManager")` — or just `RmlManager`
in GDScript). Owns the RmlUi library lifecycle and process-wide registries.

```gdscript
ensure_initialized()            # explicit init for headless/tool use (contexts do it automatically)
is_initialized() -> bool
get_context_count() -> int
get_info() -> Dictionary

load_font(path: String) -> bool          # global font load (same dedup as context loads)
get_loaded_fonts() -> Array

register_texture(name, texture) -> bool  # global "texture://name", visible to all contexts
unregister_texture(name) -> bool
get_texture(name) -> Texture2D
has_texture(name) -> bool

set_default_rcss(rcss) / get_default_rcss()
set_default_rcss_enabled(enabled) / is_default_rcss_enabled()

get_recent_log() -> Array        # last 32 RmlUi log entries: {level: int, message: String}
clear_recent_log()
get_supported_rcss_properties() -> PackedStringArray   # every registered property + shorthand

signal rml_log(level: int, message: String)   # 1=error 2=assert 3=warning 4=info 5=debug
```

> ⚠ **`rml_log` connections:** use method Callables owned by Nodes, or
> disconnect before quitting. The singleton is destroyed after GDScript
> teardown — a still-connected **lambda** crashes the process at exit.

---

## RmlElementHandle

RefCounted wrapper around a live `Rml::Element`, returned by
`RmlContext.get_element_by_id()`. Cheap to hold; **invalidated when its
document unloads or reloads** — always re-query after reloads and check
`is_valid()`.

```gdscript
is_valid() -> bool
get_id() -> String / set_id(id)
get_tag_name() -> String
get_inner_rml() -> String / set_inner_rml(rml)
get_outer_rml() -> String
get_child_count() -> int
get_position() -> Vector2 / get_size() -> Vector2   # border box, after layout (px)
click()                                             # programmatic click (fires inline handlers)

get_attribute(name, default_value := "") -> String
set_attribute(name, value) / remove_attribute(name) / has_attribute(name) -> bool

get_property(name) -> String             # computed CSS property
set_property(name, value) -> bool / remove_property(name)
set_class(class_name, activate) / is_class_set(class_name) -> bool
set_pseudo_class(pseudo_class, activate) / is_pseudo_class_set(pseudo_class) -> bool

add_event_listener(event_type, callable, in_capture_phase := false)
```

---

## Inline GDScript

Documents can carry behavior — see the [authoring guide](AUTHORING.md#inline-gdscript)
for the full model. API summary:

- `<script>` blocks compile at document load; instances are created lazily on
  the first dispatched event (loading a document never runs user code).
- `<script src="res://path.gd">` loads an external script (full IDE support).
- Any `on*` attribute with `gdscript:method_name` dispatches to: the document's
  script blocks → the RmlContext node's attached script → its parent node.
  Handlers receive the event Dictionary.
- A block declaring `var rml_context` receives the owning RmlContext before
  its first call.
- `RmlContext.get_document_script()` exposes the block instance to game code
  (signals, direct calls).
