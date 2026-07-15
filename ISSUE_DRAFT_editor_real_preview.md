# Enhancement: a real (not partial) editor preview — editor-registered decorator shaders, opt-in script execution, and code-provided mock data

> Draft for a new GitHub issue. Follow-up to #29, which was filed after noticing
> that inline `<script>` blocks *partially* run in the editor preview. The #29
> fix was a **bug fix**: the editor froze while editing documents that used
> decorator shaders (a per-element `push_warning` storm on every keystroke — see
> below), and arbitrary inline GDScript could run in the editor. That work made
> the preview **safe and responsive**; this issue tracks making it **complete**.

## Background — what #29 changed

- **Decorator shaders no longer freeze the editor.** `decorator: shader("name")`
  shaders are registered from GDScript (`register_decorator_shader` /
  `register_decorator_material`), which never runs in the editor — so every
  decorated element failed to compile and emitted a warning *per element, on
  every keystroke* (the live preview + diagnostics both reload the document as
  you type). `push_warning` is expensive in the editor (backtrace + Output dock +
  remote debugger), so this froze editing. Fixed by deduping the notice per
  shader name and routing it through `notify_log` instead of the console.
  **Net effect today:** decorated elements render as plain geometry in the
  editor — correct layout, no shader.
- **Inline scripts are gated off in the editor by default**, with a per-session
  **"Run inline scripts"** checkbox in the preview panel (off by default; flips a
  per-context flag checked in `GodotScriptDocument::_ensure_instance`).

So the preview is currently *partial by design*: no shaders, no game logic
unless explicitly opted in, and data models only via the inspector's
`editor_mock_data` dictionary. This issue is about closing those gaps.

## Goals

### 1. Editor-registered decorator shaders (preview shaders without running the game)

Let documents preview their shader decorators without the game's `_ready()`
registration. Options, roughly in order of preference:

- **Resolve `decorator: shader("name")` to a `.gdshader` by convention/manifest.**
  e.g. an `editor_decorator_shaders` dictionary on `RmlContext` (`{ "plasma":
  "res://ui/plasma.gdshader" }`), mirroring `editor_mock_data` — the preview
  registers these before load. Inspector-only, no code path.
- **Auto-discover** `.gdshader` files by name in the document's directory.
- **A `@tool`-style preview hook** (see goal 2) that calls
  `register_decorator_shader` in the editor.

Until then, the plain-geometry fallback stands; this just makes the showcase
(`examples/showcase/decorator_shaders`) and rarity-FX-style grids previewable.

### 2. Safe opt-in script execution for previews

The "Run inline scripts" checkbox exists but is a blunt instrument — any infinite
loop or blocking call still hangs the editor when enabled (by design, it's the
user's choice). To make it genuinely usable:

- **Sandboxing / watchdog:** time-box a handler; abort + report instead of hang.
- **Editor-aware lifecycle:** a documented `_on_editor_preview()` entry point (or
  honoring a `@tool`-like marker on the `<script>` block) so authors can populate
  preview state deliberately, separate from runtime `onload`.
- **Mock the runtime surface:** `get_tree()`, autoloads, and singletons that
  inline scripts reach via `rml_context` are absent/different in the editor —
  decide what's stubbed vs. available.

### 3. Code-provided mock data (the original #29 ask)

> "Something that would be very useful for previews is to be able to provide
> mockup data via code instead of editor dictionary which to be honest is a bit
> of a pain to provide."

`editor_mock_data` (a nested Dictionary in the inspector) is tedious for
anything non-trivial. Allow a script to build the preview's data models instead:

- A preview hook (goal 2's `_on_editor_preview()`) that calls
  `create_data_model_from_dict` / `bind_data_array` directly, OR
- A referenced `@tool` `.gd` "mock provider" resource on the `RmlContext` whose
  method returns the model dict, run by the preview panel before load.

This also composes with goal 1 (the same hook can register shaders).

## Non-goals / notes

- This is explicitly the "potential" from #29, decoupled from the freeze fix so
  the bug fix could ship immediately.
- Anything that runs author code in the editor must stay **opt-in** and guarded —
  the #29 freeze is a reminder that the editor is unforgiving of blocking work.

## Acceptance ideas

- The `decorator_shaders` showcase previews with live shaders (no game run).
- A document can populate a non-trivial `data-for` list in the preview from code,
  with no inspector dictionary.
- Enabling preview scripts on a document with a runaway loop reports/aborts
  instead of freezing the editor.
