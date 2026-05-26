# rmlui-godot

[![Godot 4.2+](https://img.shields.io/badge/Godot-4.2%2B-478cbf?logo=godotengine&logoColor=white)](https://godotengine.org/)
[![RmlUi](https://img.shields.io/badge/RmlUi-6.3-orange)](https://github.com/mikke89/RmlUi)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![AI: Claude Opus 4.6](https://img.shields.io/badge/AI-Claude_Opus_4.6-cc785c?logo=anthropic&logoColor=white)](https://anthropic.com)

A GDExtension plugin that integrates [RmlUi](https://github.com/mikke89/RmlUi) into [Godot 4.2+](https://godotengine.org/), giving you CSS/HTML-style UI with full GDScript interop.

> **Status:** Active development. The plugin powers production game UI in [CaveCrawler](https://github.com/Brokenchin) and is being extracted into this standalone repository.

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
- Array binding: `bind_data_array`, `push_data_array_item`, `remove_data_array_item`, `set_data_array_item`, `clear_data_array`
- Data events: `bind_data_event()` routes RmlUi data events to GDScript `Callable`
- Dirty tracking: `dirty_data_variable()` / `dirty_all_variables()`

### DOM Events & Element Access
- `add_event_listener(element_id, event_type, callable)` — capture phase supported
- `get_element_by_id()` returns `RmlElementHandle`
- Set/remove CSS properties, toggle classes, read/write attributes
- Inner RML manipulation (`set_element_inner_rml`, `get_element_outer_rml`)

### Custom Elements
- `register_custom_element(tag, on_create, on_attribute_change)` — extend RML with custom tags backed by GDScript callables

### Drag & Drop
Bridges RML elements to Godot's native drag system (`_get_drag_data` / `_can_drop_data` / `_drop_data`). Sources and targets must be registered from GDScript — they cannot be defined in RML/RCSS alone.
- `register_drag_source(element_id, payload_builder, ghost_builder)` — `payload_builder` returns the drag data, optional `ghost_builder` returns custom ghost RML (auto-generated from computed styles if omitted)
- `register_drop_target(element_id, drop_handler)` — `drop_handler` receives the element id and drag data on drop
- Signals: `rml_drag_started`, `rml_drop_received`
- Ghost is a real transient `RmlContext` used as Godot's `drag_preview`
- **Cross-system interop:** because it bridges to Godot's native drag, items can be dragged between RmlUI contexts and native Godot Controls seamlessly — drag from a Godot node into an RML panel or vice versa

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

```bash
git clone https://github.com/Brokenchin/rmlui-godot.git
cd rmlui-godot
mkdir dependencies
cd dependencies
git clone https://github.com/godotengine/godot-cpp.git -b godot-4.5-stable
git clone https://github.com/Brokenchin/RmlUi-multicontext.git -b multicontext_experiment RmlUi
```

### 2. Configure and build

```bash
cd ..
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The built library lands in `project/addons/rmlui-godot/bin/`.

### 3. Use in your project

Copy the `project/addons/rmlui-godot/` folder into your Godot project's `addons/` directory.

### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `RMLUI_GODOT_STANDALONE` | `ON` | `ON` = shared library for GDExtension, `OFF` = static lib for embedding in a larger build |
| `GODOT_CPP_DIR` | — | Path to godot-cpp (auto-detects `dependencies/godot-cpp`) |
| `RMLUI_DIR` | — | Path to RmlUi (auto-detects `dependencies/RmlUi`) |

## Quick Start

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
    rml.set_data_variable("ui", "player_name", "Adventurer")
    rml.dirty_data_variable("ui", "player_name")
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

## Architecture

```
rmlui-godot/
├── src/rmlui_godot/          # C++ plugin source
│   ├── register_types.*      # GDExtension entry point
│   ├── RmlManager.*          # Singleton: fonts, textures, lifecycle
│   ├── RmlContext.*           # Per-instance Control node
│   ├── RmlElementHandle.*    # Type-safe element wrapper
│   ├── RmlGD.hpp             # GD class macros (zero dependencies)
│   ├── GodotRenderInterface.*    # CanvasItem renderer
│   ├── GodotSystemInterface.*    # Time, logging
│   ├── GodotFileInterface.*      # res:// filesystem bridge
│   ├── GodotFontInterface.*      # FreeType font loading
│   ├── GodotEventListener.*      # Event → Callable routing
│   ├── GodotEventListenerInstancer.*
│   └── GodotElementInstancer.*   # Custom element factory
├── project/                  # Example Godot project
│   ├── addons/rmlui-godot/   # Plugin (bin + .gdextension)
│   └── project.godot
├── CMakeLists.txt
├── LICENSE                   # MIT
└── README.md
```

## Capabilities

| Feature | Status |
|---------|--------|
| Multiple contexts | Safe — targeted cleanup via [fork API](https://github.com/Brokenchin/RmlUi-multicontext) |
| Data binding | Scalars, arrays, events, batch dict setup |
| Custom elements | GDScript callables for create + attribute change |
| Drag & drop | Bridges to Godot's native drag — GDScript registration, auto-ghost, cross-system interop |
| DOM manipulation | Get/set properties, classes, attributes, inner RML |
| Texture registry | Global (RmlManager) + per-context |
| Hot reload | Per-document and all-documents |
| Stylesheet injection | Runtime RCSS injection |
| Font handling | Global + per-context, premultiplied alpha |

## Planned

| Feature | Notes |
|---------|-------|
| Inline GDScript in RML | Execute GDScript blocks embedded directly in `.rml` files |
| Editor integration | Live preview of RML documents in the Godot editor — interactive, clickable, draggable |
| Gamepad / input actions | Better input handling — register Godot input actions to forward to RmlUi instead of raw key events |
| Documentation & wiki | Usage guides, API reference, and example walkthroughs |

## License

MIT — see [LICENSE](LICENSE).

RmlUi is licensed under the MIT license. See the [RmlUi repository](https://github.com/mikke89/RmlUi) for details.
