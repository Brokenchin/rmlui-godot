# rmlui-godot

[![Godot 4.2+](https://img.shields.io/badge/Godot-4.2%2B-478cbf?logo=godotengine&logoColor=white)](https://godotengine.org/)
[![RmlUi](https://img.shields.io/badge/RmlUi-6.3-orange)](https://github.com/mikke89/RmlUi)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![AI: Claude Opus](https://img.shields.io/badge/AI-Claude_Opus-cc785c?logo=anthropic&logoColor=white)](https://anthropic.com)

A GDExtension plugin that integrates [RmlUi](https://github.com/mikke89/RmlUi) into [Godot 4.2+](https://godotengine.org/), giving you CSS/HTML-style UI with full GDScript interop.

> **Status:** v1.0.0 released — grab the prebuilt addon from
> [Releases](https://github.com/Brokenchin/rmlui-godot/releases) (Windows / Linux / macOS).

## Documentation

| Doc | What's in it |
|---|---|
| [API Reference](docs/API.md) | Every GDScript-facing method, property and signal of `RmlContext`, `RmlManager`, `RmlElementHandle` — plus the inline-GDScript dispatch model and thread-safety contract |
| [RML Authoring Guide](docs/AUTHORING.md) | Writing documents: structure, data binding, inline GDScript, drag & drop, Godot textures/shaders, editor tooling, pitfalls |
| [RCSS Reference](docs/RCSS.md) | All RCSS properties registered by this build, differences from web CSS, decorators, RmlUi-specific properties |

## Features

### Core
- **RmlContext** node (extends `Control`) — drop into any scene, auto-sizes to parent
- **RmlManager** singleton — global font cache, shared texture registry, lifecycle management
- **RmlElementHandle** — type-safe element references with full DOM manipulation

### Document Management
- Load / unload / reload `.rml` documents at runtime
- Multiple documents per context
- Auto-load via inspector (`document_path`, `font_paths` properties)
- Hot-reload with `reload_document()` / `reload_all_documents()`
- Runtime stylesheet injection (`inject_stylesheet()`)

### Data Binding
- `create_data_model` / `bind_data_variable` / `set_data_variable` / `get_data_variable`
- Batch setup via `create_data_model_from_dict()` / `update_data_model()`
- Array binding: `bind_data_array`, `push_data_array_item`, `remove_data_array_item`, `set_data_array_item`, `clear_data_array` — scalars **or arrays of dictionaries** (struct rows: `data-for="slot : slots"` → `slot.icon`, `slot.count`)
- Data events: `bind_data_event()` routes RmlUi data events to GDScript `Callable`
- Dirty tracking: `dirty_data_variable()` / `dirty_all_variables()`

### DOM Events & Element Access
- `add_event_listener(element_id, event_type, callable)` — capture phase supported
- `get_element_by_id()` returns `RmlElementHandle`
- Set/remove CSS properties, toggle classes, read/write attributes
- Inner RML manipulation (`set_element_inner_rml`, `get_element_outer_rml`)

### Custom Elements
- `register_custom_element(tag, on_create, on_attribute_change)` — extend RML with custom tags backed by GDScript callables

### Inline GDScript
Write behavior directly inside `.rml` documents — no `.gd` file needed:
- `<script>` blocks are full GDScript classes (vars, funcs, signals, `await`); `<script src="res://*.gd">` for IDE-grade editing
- `onclick="gdscript:method"` on any `on*` event attribute; `var rml_context` auto-injected
- Game-code bridge: `get_document_script()` / `get_document_scripts()` — connect to block signals, call block methods
- Error lines map to the `.rml` file — parse errors and runtime stack frames alike

### Input Actions & Gamepad Navigation
- `input_actions` — watched InputMap actions dispatch to `<script>` blocks (`_on_input_action`) and the `rml_input_action` signal
- `gamepad_navigation` — Godot's `ui_*` actions drive RmlUi's focus engine: spatial navigation (`nav: auto`), tab order, accept-to-click. D-pad/stick work out of the box, all rebindable
- RmlUi debugger overlay on **F10** (configurable `debugger_toggle_key`, or `toggle_debugger()`)

### Editor Tooling
Ships in the addon — enable the "RmlUI-Godot" plugin:
- Syntax highlighting for `.rml` and `.rcss`, including embedded `<style>`/`style=""` RCSS and `<script>` GDScript regions
- Autocomplete: RCSS properties/values (live from the engine), RML tags/attributes with auto-closing tags, GDScript member completion (`rml_context.` lists real methods via reflection)
- **Live preview panel**: select an RmlContext to render its document; unsaved buffer edits apply live; mouse hover/click/scroll work in-panel; `editor_mock_data` feeds `data-for`/`{{ }}` bindings
- Inline diagnostics: parse errors tint the offending line + error bar, ~0.5s after you stop typing
- Inspector: configuration warnings, Edit/Create document buttons, live context stats
- Documents render in the editor's 2D viewport (set `document_path` — zero code)

### Drag & Drop
Bridges RML elements to Godot's native drag system (`_get_drag_data` / `_can_drop_data` / `_drop_data`). Sources and targets must be registered from GDScript — they cannot be defined in RML/RCSS alone.
- `register_drag_source(element_id, payload_builder, ghost_builder)` — `payload_builder` returns the drag data, optional `ghost_builder` returns custom ghost RML (auto-generated from computed styles if omitted)
- `register_drop_target(element_id, drop_handler)` — `drop_handler` receives the element id and drag data on drop
- Signals: `rml_drag_started`, `rml_drop_received`
- Ghost is a real transient `RmlContext` used as Godot's `drag_preview`
- **Cross-system interop:** because it bridges to Godot's native drag, items can be dragged between RmlUI contexts and native Godot Controls seamlessly — drag from a Godot node into an RML panel or vice versa

### Hover Bridge
Mirrors the drag bridge for tooltips that must escape the source document's clipping (ancestor `overflow` *and* the context viewport). Resolved by element id at event time, so it works for slots streamed in via `set_element_inner_rml`.
- Signals: `rml_element_hovered(element_id, global_position)` / `rml_element_unhovered(element_id)` — fire on enter/leave for the nearest ancestor carrying an `id`
- `get_hovered_element_id()` — poll the current hovered id (option B, for following tooltips)
- Render the tooltip in a separate, screen-sized overlay context so it draws unclipped beside the hovered slot

### Rendering
- Premultiplied alpha pipeline — correct blending for fonts, sprites, and textures
- Per-context `GodotRenderInterface` with Godot's `CanvasItem` drawing API
- DPI-aware scaling (`dp_ratio` property)
- Shared texture registry — register `Texture2D` resources by name, reference in RML via `<img src="name">`

### Font Handling
- Load `.ttf` / `.otf` via `load_font()` (global) or `load_font_face()` (per-context)
- FreeType integration through RmlUi's font engine
- Font atlas premultiply for correct alpha blending

### Multi-Context Safety
- Multiple `RmlContext` nodes in one scene tree — each owns its own RmlUi context
- Context destruction doesn't corrupt surviving contexts' fonts or textures
- Uses a [forked RmlUi](https://github.com/Brokenchin/RmlUi-multicontext) with targeted render-manager cleanup API

## Requirements

- **Godot 4.2+** (tested with 4.5)
- **godot-cpp** (matching your Godot version)
- **RmlUi** — recommended: [multicontext fork](https://github.com/Brokenchin/RmlUi-multicontext) (`multicontext_experiment` branch) for multi-context safety. Upstream RmlUi works for single-context use.
- **CMake 3.20+**
- **C++20 compiler** (MSVC 2022, GCC 12+, Clang 15+)

## Building

### 1. Clone with dependencies

Dependencies (godot-cpp and RmlUi) are included as git submodules.

```bash
git clone --recursive https://github.com/Brokenchin/rmlui-godot.git
cd rmlui-godot
```

Or if already cloned: `git submodule update --init` (dependencies) —
examples are fetched automatically at configure time, or manually via
`git submodule update --init --checkout examples`.

### 2. Configure and build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The built library lands in `project/addons/rmlui-godot/bin/`.

### 3. Use in your project

Copy the `project/addons/rmlui-godot/` folder into your Godot project's `addons/` directory.

### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `RMLUI_GODOT_STANDALONE` | `ON` | `ON` = shared library for GDExtension, `OFF` = static lib for embedding |
| `GODOT_PROJECT_ROOT` | `project/` | Path to Godot project directory (where project.godot lives). Addon files are installed to `<root>/addons/rmlui-godot/` |
| `RMLUI_GODOT_ADDON_NAME` | `rmlui-godot` | Plugin folder name under addons/ |
| `RMLUI_GODOT_INSTALL_EXAMPLES` | `ON` (standalone) / `OFF` (embedded) | Fetch and install examples submodule |
| `GODOT_CPP_DIR` | — | Override path to godot-cpp (auto-detects submodule) |
| `RMLUI_DIR` | — | Override path to RmlUi (auto-detects submodule) |

### Embedded builds

For projects that compile RmlUI-Godot as a static library (e.g. as part of a larger GDExtension):

```cmake
set(RMLUI_GODOT_STANDALONE OFF)
set(GODOT_PROJECT_ROOT "${CMAKE_SOURCE_DIR}/path/to/your/godot/project")
add_subdirectory(path/to/rmlui-godot)
target_link_libraries(your_target PRIVATE rmlui_godot)
```

CMake will copy only the editor plugin files (GDScript, plugin.cfg, base.rcss) to your Godot project — no .gdextension or binaries, since C++ is statically linked by the host.

## Quick Start

Zero-code: drop an `RmlContext` node in a scene and set its
**Document Path** + **Font Paths** in the inspector — it renders in the
editor immediately. Or from script:

```gdscript
# Your RmlContext node is in the scene tree (add via editor or code)
@onready var rml: RmlContext = $RmlContext

func _ready():
    # Load fonts
    rml.load_font_face("res://fonts/NotoSans-Regular.ttf")

    # Set up a data model
    rml.create_data_model("ui")
    rml.bind_data_variable("ui", "player_name", "Hero")
    rml.bind_data_variable("ui", "health", 100)

    # Load the document
    rml.load_document("res://ui/hud.rml")

    # Listen for events
    rml.add_event_listener("start_button", "click", _on_start_clicked)

func _on_start_clicked(event: Dictionary):
    rml.set_data_variable("ui", "player_name", "Adventurer")  # auto-dirties
```

```html
<!-- res://ui/hud.rml -->
<rml>
<head>
    <link type="text/rcss" href="hud.rcss"/>
</head>
<body>
    <h1>{{ player_name }}</h1>
    <div id="health-bar" style="width: {{ health }}%"></div>
    <button id="start_button">Start</button>
</body>
</rml>
```

## Examples

Example scenes are available in a [separate repository](https://github.com/Brokenchin/rmlui-godot-examples). When building standalone, pass `-DRMLUI_GODOT_INSTALL_EXAMPLES=ON` (default) to fetch and install them to `examples/` in your Godot project root.

### basic/ — Getting Started
| Example | Demonstrates |
|---------|-------------|
| `hello_world` | Document loading and font setup |
| `data_binding` | Data models, variable binding, data events |
| `events` | Click, hover, class toggle, DOM manipulation |
| `list_binding` | Array binding with data-for loops |
| `inline_script` | Interactive counter — all behavior in the document `<script>` block |
| `gamepad_nav` | Keyboard/gamepad focus navigation, accept-to-click, `ui_cancel` handling |

### advanced/ — Deeper Features
| Example | Demonstrates |
|---------|-------------|
| `custom_elements` | Custom RML tags backed by GDScript callables |
| `textures` | Spritesheets, decorators, CSS animations, transitions |
| `drag_and_drop` | Cross-context drag using native C++ drag API |
| `inline_drag` | Full drag & drop implemented inline, with a signal bridged to Godot |

### showcase/ — Unique Selling Points
| Example | Demonstrates |
|---------|-------------|
| `visual_parity` | Side-by-side Godot native vs RmlUI + native drag interop |
| `decorator_shaders` | Custom Godot shaders as RCSS decorators |
| `font_comparison` | Text render mode comparison (SubPixel / Godot Native / RmlUI Native) |
| `font_effects` | Glow, outline, shadow, blur font-effect layers |

### stress/ — Stability Proof
| Example | Demonstrates |
|---------|-------------|
| `context_churn` | Create/destroy contexts rapidly — proves cleanup works |
| `multi_context` | 4 simultaneous contexts with heavy DOM content |
| `error_recovery` | Malformed RML, missing resources, graceful degradation |
| `scissor_limits` | Clipping edge cases (CPU + GPU scissor) |
| `zindex_stacking` | Deep z-index stacking correctness |

## Architecture

```
rmlui-godot/
├── src/rmlui_godot/          # C++ plugin source
├── project/                  # Standalone Godot project
│   ├── addons/rmlui-godot/   # Plugin (bin + editor tools + shaders;
│   │                         #   examples synced here by the build)
│   ├── tests/                # Automated test suite (tests/run_all.sh)
│   └── project.godot
├── examples/                 # Examples submodule (rmlui-godot-examples)
│   ├── basic/ advanced/ showcase/ stress/
│   └── fonts/ assets/
├── docs/                     # API.md, AUTHORING.md, RCSS.md
├── dependencies/             # Git submodules
│   ├── godot-cpp/            # Godot C++ bindings
│   └── RmlUi/                # RmlUi multicontext fork
├── .github/workflows/        # CI: build + release on v* tags
├── CMakeLists.txt
├── LICENSE
└── README.md
```

## Capabilities

| Feature | Status |
|---------|--------|
| Multiple contexts | Safe — targeted cleanup via [fork API](https://github.com/Brokenchin/RmlUi-multicontext) |
| Data binding | Scalars, arrays, events, batch dict setup |
| Custom elements | GDScript callables for create + attribute change |
| Drag & drop | Bridges to Godot's native drag — GDScript registration, auto-ghost, cross-system interop |
| Hover bridge | `rml_element_hovered`/`unhovered` signals + `get_hovered_element_id()` for tooltips drawn in an overlay context |
| DOM manipulation | Get/set properties, classes, attributes, inner RML |
| Texture registry | Global (RmlManager) + per-context |
| Hot reload | Per-document and all-documents |
| Stylesheet injection | Runtime RCSS injection |
| Font handling | Global + per-context, premultiplied alpha, dedup |
| Inline GDScript | `<script>` blocks, `gdscript:` handlers, signal bridge, rml-accurate error lines |
| Input actions | InputMap actions → documents + signal |
| Gamepad navigation | Spatial focus, tab order, accept-to-click via `ui_*` actions |
| Editor tooling | Highlighting, autocomplete, live preview, diagnostics, inspector |
| Test suite | 21 automated tests — `tests/run_all.sh` |

## Planned

| Feature | Notes |
|---------|-------|
| File icons | Custom `.rml`/`.rcss` icons in the FileSystem dock |
| Navigation polish | Held-key repeat for gamepad nav, per-panel focus memory |
| Inline-script breakpoints | Requires registering a `ScriptLanguageExtension` — research stage |

## License

MIT — see [LICENSE](LICENSE).

RmlUi is licensed under the MIT license. See the [RmlUi repository](https://github.com/mikke89/RmlUi) for details.
