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
| `editor_mock_data` | Dictionary | `{model_name: {var_name: value}}` — applied **only by the editor preview panel** so `data-for`/`{{ }}` render without running the game. Arrays become data arrays. |

Configuration warnings (⚠ in the scene tree) flag a missing `document_path`,
nonexistent document/font files, and no-fonts-anywhere setups.

### Signals

| Signal | Args | When |
|---|---|---|
| `rml_drag_started` | `element_id: String, payload: Dictionary` | A registered drag source started dragging. |
| `rml_drop_received` | `element_id: String, data: Dictionary` | A registered drop target received a drop. |

### Documents

```gdscript
load_document(path: String)
load_document_from_string(rml_text: String, alias_path := "memory://document") -> bool
reload_document(path: String) -> bool
reload_all_documents()
unload_document(path: String) -> bool
get_loaded_documents() -> Array          # paths/aliases
get_document_script(document_path := "") -> Variant   # see Inline GDScript
```

- `load_document_from_string` parses from memory; `alias_path` is used for
  error messages **and** to resolve relative `<link href>` — pass the real
  path when the text mirrors a file.
- `reload_document` preserves data models and re-dirties all variables.
- `get_document_script("")` returns the first `<script>` block instance of any
  loaded document (forcing its lazy instantiation), or `null`. Connect to its
  signals or call its methods from game code.

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
Arrays are arrays-of-strings on the RmlUi side; non-string values are
stringified. All mutators auto-dirty.

### Elements & events

```gdscript
get_element_by_id(id: String) -> RmlElementHandle    # searches all loaded documents
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
