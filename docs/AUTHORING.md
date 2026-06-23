# RML Authoring Guide

How to write `.rml` documents for rmlui-godot — including the Godot-specific
extensions (inline GDScript, data binding helpers, `texture://` and shader
decorators) and the editor tooling that makes the loop fast.

RML is RmlUi's XML dialect of HTML; RCSS is its CSS dialect. Upstream
references: [RML](https://mikke89.github.io/RmlUiDoc/pages/rml.html) ·
[RCSS](https://mikke89.github.io/RmlUiDoc/pages/rcss.html). This guide covers
what's different or extended here.

## Document skeleton

```xml
<rml>
<head>
	<title>My Screen</title>
	<link type="text/rcss" href="res://addons/rmlui-godot/base.rcss" />
	<link type="text/rcss" href="my_screen.rcss" />   <!-- relative to this file -->
	<style>
		/* inline RCSS — same cascade position as a <link> here */
		body { font-family: "Noto Sans"; font-size: 16dp; color: #eee; }
	</style>
</head>
<body>
	<div id="panel">
		<h1>Hello</h1>
		<p style="color: #9cf;">Inline styles work too.</p>
	</div>
</body>
</rml>
```

- RML is **strict XML**: every tag closes (`<br/>`, `</div>`), attribute
  values are quoted, `&` `<` `>` in text need entities.
- Paths: `res://` absolute or relative-to-document both work in `href`/`src`.
- Fonts must be loaded before text renders — set `font_paths` on the
  RmlContext (preferred: also works in the editor) or call `load_font_face()`.
- Sizes: prefer `dp` (scaled by `dp_ratio`) over `px` for resolution
  independence.

## Loading: properties vs code

**Prefer the inspector properties** (`document_path`, `font_paths`). They are
zero-code, render in the editor's 2D viewport, and power the preview panel.
Script-driven `load_document()` calls don't run in the editor — the node will
show a configuration warning explaining exactly that.

`document_path` loads **after the scene's `_ready` cascade** (deferred), so
the natural pattern just works: create data models / register custom elements
in `_ready`, and the document binds them on load. Code that needs the loaded
document (attaching element listeners, populating content) should
`await get_tree().process_frame` first.

## Data binding

RmlUi's reactive layer. The model must exist **before** the document loads:

```gdscript
func _ready():
    var ctx: RmlContext = $RmlContext
    ctx.create_data_model_from_dict("hud", {"hp": 100, "name": "Player"})
    ctx.bind_data_array("hud", "log_lines", [])
    ctx.load_document("res://ui/hud.rml")   # or set document_path AFTER models

func damage(amount: int):
    hp -= amount
    ctx.set_data_variable("hud", "hp", hp)   # document updates itself
```

```xml
<body data-model="hud">
	<div>HP: {{ hp }} / 100</div>
	<div data-if="hp < 25" class="warning">LOW HEALTH</div>
	<div data-for="line : log_lines">{{ line }}</div>
	<button data-event-click="heal">Heal</button>   <!-- bind_data_event("hud", "heal", ...) -->
</body>
```

`{{ }}` accepts expressions — arithmetic, comparisons, ternaries:
`{{ hp > 50 ? 'fine' : 'hurt' }}`. This is RmlUi's own expression language
evaluating against the data model (not GDScript).

**Editor preview:** set `editor_mock_data` on the node
(`{"hud": {"hp": 80, "log_lines": ["a", "b"]}}`) and bindings render without
running the game — both in the preview panel and the 2D viewport.

## Inline GDScript

Self-contained interactive documents — no `.gd` file required.

```xml
<head>
	<script>
	var rml_context        # auto-injected: the owning RmlContext node
	var count := 0

	signal threshold_reached(value: int)   # game code can connect — see below

	func _on_load(_event):
		rml_context.create_data_model_from_dict("ui", {"count": 0})

	func _add(_event):
		count += 1
		rml_context.set_data_variable("ui", "count", count)
		if count == 10:
			threshold_reached.emit(count)
	</script>
	<!-- or external, with full IDE support: -->
	<!-- <script src="res://ui/my_screen.gd"></script> -->
</head>
<body data-model="ui" onload="gdscript:_on_load">
	<button onclick="gdscript:_add">+</button>
	<span>{{ count }}</span>
</body>
```

### Input actions

Script blocks are not Nodes — `_input`/`_process` never fire. For game
actions (InputMap), set the context's `input_actions` property and implement
`_on_input_action` in the block:

```gdscript
$RmlContext.input_actions = ["inventory_toggle", "ui_cancel"]
```

```xml
<script>
var rml_context
var open := false

func _on_input_action(action: String, pressed: bool):
	if action == "inventory_toggle" and pressed:
		open = not open
		rml_context.set_element_class("panel", "hidden", not open)
</script>
```

The same press/release also reaches game code via the context's
`rml_input_action(action, pressed)` signal — use whichever side owns the
behavior. Events arrive through `_unhandled_input`, so UI controls that
consume input (text fields etc.) keep priority.

Rules of the model:

- A block is a **full GDScript class** (implicit `RefCounted`): vars, funcs,
  signals, inner classes, `await`, `load()`, autoload access via
  `rml_context.get_node("/root/MyAutoload")`.
- Compiles at document load (errors report the document path). The instance is
  created **lazily on the first dispatched event** — loading never runs user
  code. Hot reload recompiles changed blocks; unchanged blocks are reused.
- `gdscript:method` works on any `on*` event attribute. Dispatch order:
  script blocks → the RmlContext's attached script → its parent node.
  Handlers get one argument: the event Dictionary.
- Bridge out with signals: `$RmlContext.get_document_script().threshold_reached.connect(...)`.
- Instance state dies with the document (reload = fresh state). Keep
  persistent state in data models or game-side.
- No `_process`/`_ready` (not a Node) and no debugger breakpoints — put
  complex logic in a `<script src="...">` file instead.

## Gamepad & keyboard navigation

Set `gamepad_navigation = true` on the RmlContext and opt elements in via RCSS:

```css
button { tab-index: auto; nav: auto; }   /* focusable + spatial nav */
button:focus { border: 2dp #6699ee; }    /* the focus indicator */
```

That's the whole setup. Godot's `ui_*` actions (D-pad/stick/arrows out of the
box, rebindable in the InputMap) then drive RmlUi's built-in focus engine:
arrows = nearest-element spatial navigation, Tab = document order,
Accept = click the focused element (fires `onclick` — inline `gdscript:`
handlers included), Cancel = forwarded (handle via `input_actions` +
`_on_input_action`).

Fine control: `nav-up/down/left/right` accept `none`, `auto`, `horizontal`,
`vertical`, or `#element-id` for explicit wiring; `nav: vertical` on list
items keeps focus inside a column. See `examples/basic/gamepad_nav`.

## Drag & drop

Registration is API-driven (works from inline scripts too):

```gdscript
ctx.register_drag_source("item-1", _build_payload)        # payload_builder(id, pos) -> Dictionary
ctx.register_drop_target("slot-3", _on_drop)              # drop_handler(target_id, data)
```

```css
.item { drag: clone; }          /* RCSS opts the element into dragging */
.item:drag { visibility: hidden; }
.slot:drag-over { border: 2dp #5588bb; }
```

Bridges Godot's native drag — drags cross between RmlContexts and native
Controls freely. See `examples/advanced/inline_drag` for a fully inline
implementation with a signal bridged back to a Godot Label.

## Hover bridge (unclipped tooltips)

A tooltip authored inside the document (`.slot:hover .card`) is clipped by
ancestor `overflow` *and* this context's viewport, so it gets cut off at the
panel edge. To draw it unclipped, render the tooltip in a **separate,
screen-sized overlay context** and drive it from the hover signals:

```gdscript
inventory_ctx.rml_element_hovered.connect(_on_slot_hovered)
inventory_ctx.rml_element_unhovered.connect(_on_slot_unhovered)

func _on_slot_hovered(element_id: String, global_position: Vector2) -> void:
    var item := items[element_id]
    overlay_ctx.set_element_inner_rml("tooltip", _card_rml(item))
    overlay_ctx.set_element_property("tooltip", "left", str(global_position.x) + "px")
    overlay_ctx.set_element_property("tooltip", "top", str(global_position.y) + "px")

func _on_slot_unhovered(_element_id: String) -> void:
    overlay_ctx.set_element_property("tooltip", "display", "none")
```

The id reported is the nearest ancestor with an `id`, resolved at event time —
so it works for slots streamed in via `set_element_inner_rml`, where
data-binding attributes don't reliably bind. Signals fire once on enter and
once on leave; poll `ctx.get_hovered_element_id()` on mouse motion instead if
you want a tooltip that follows the cursor.

## Godot textures & shader decorators

```gdscript
RmlManager.register_texture("portrait", my_texture)   # global
ctx.register_texture("minimap", viewport_texture)     # this context only
ctx.register_decorator_shader("scanlines", my_shader)
```

```css
.portrait { decorator: image("texture://portrait"); }
.fancy    { decorator: shader(scanlines); }
```

## Editor tooling

Everything ships in the addon (`plugin.cfg` → enable "RmlUI"):

- **Syntax highlighting** for `.rml` and `.rcss` (including embedded `<style>`,
  `style=""` and `<script>` regions). Files open in the script editor from the
  FileSystem dock. *Files opened before the plugin existed may be cached as
  "Plain Text" — switch once via the script editor's Edit ▸ Syntax Highlighter.*
- **Autocomplete**: RCSS properties (live list from the engine) and values;
  RML tags, per-tag attributes, `data-*`/`on*`; typing `>` auto-inserts the
  closing tag; `</` suggests the innermost unclosed tag.
- **Inline diagnostics**: parse errors tint the offending line and show in an
  error bar under the editor, ~0.5 s after you stop typing — no save needed.
- **Live preview** (bottom panel): select an RmlContext to render its
  document; edits to open `.rml`/`.rcss` buffers apply live without saving;
  mouse hover/click/scroll work inside the panel; parse errors appear in the
  status line. `editor_mock_data` feeds bindings.
- **Inspector**: Edit/Create buttons for the document and its linked `.rcss`,
  a live status line, and a Preview shortcut.

## Pitfalls worth knowing

- **Models before documents** — `data-model="x"` resolves at load time.
- **Element handles go stale on reload** — re-query, check `is_valid()`.
- **`inject_stylesheet` wins the cascade** and stacks until the document
  reloads; for live theming prefer data-driven classes.
- **Main thread only** — marshal worker-thread results via `call_deferred`.
- **`RmlManager` signal connections from lambdas** must be disconnected before
  quit (singleton outlives GDScript teardown).
- **F10** toggles the RmlUi debugger overlay at runtime (element inspector).
  Configurable via `RmlContext.debugger_toggle_key` (0 disables) or call
  `toggle_debugger()` from code. (F8/F9 are the editor's Stop/Pause shortcuts
  for the 4.5 embedded game window — they never reach the game.)
