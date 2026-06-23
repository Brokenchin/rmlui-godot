# Feature: Ctrl/Cmd-click `<script src>` / `<link href>` paths in the .rml editor to open the target file

> Draft for a new GitHub issue. Surfaced while fixing #25 (`<script src>` path
> mangling) — the resolution logic touched there is exactly what a "jump to file"
> feature would reuse, so it's cheap to add on top.

## Summary

When editing an `.rml`/`.rcss` document in Godot's script editor, the file paths in
`<script src="…">`, `<link href="…">`, `<link type="text/template" href="…">`, and
RCSS `@import`/`url(…)` are plain text. You can't get from the document to the file it
references without manually hunting it down in the FileSystem dock.

Make those paths **navigable**: Ctrl-click (Cmd-click on macOS) — the same gesture that
jumps to a symbol definition in GDScript — should open the referenced file in the editor.
As a smaller companion, make the runtime/loader error messages that print a resource path
(e.g. `[RmlUi] <script src> is not a GDScript: res://…`) **clickable in the Output log**.

## Motivation

- Authors keep a document and its backing GDScript/RCSS side by side (the #25 repro is
  exactly this: `view_frame_doc.rml` + `view_frame_doc.gd`). Today, jumping between them
  is a manual FileSystem-dock search.
- It mirrors an idiom Godot users already know from GDScript (Ctrl-click a symbol →
  definition), so there's nothing new to learn.
- The path-resolution work is **already done** by the #25 fix. The same
  "decode RmlUi's `|`-encoding, join a relative path against the document dir, accept an
  absolute `res://`/`user://` path as-is" logic is what a click handler needs to turn the
  text under the cursor into a `res://` path. We'd lift it into one shared resolver instead
  of duplicating it.

## Proposed approach

### A. In-editor Ctrl-click navigation (primary)

`.rml`/`.rcss` buffers are edited in the script editor's `CodeEdit` (see
`rmlui_editor_plugin.gd::_ensure_completion_hook`, which already identifies "our" buffers
by their syntax highlighter and attaches completion + diagnostics to the `CodeEdit`).

`CodeEdit`/`TextEdit` already exposes the exact hooks Godot itself uses for clickable
symbols:

- `symbol_lookup_on_click_enabled = true`
- `symbol_validate(symbol)` signal → call `set_symbol_lookup_word_as_valid(true)` when the
  hovered text resolves to an existing file (this is what underlines it + shows the hand
  cursor).
- `symbol_lookup(symbol, line, column)` signal → resolve to a `res://` path and
  `EditorInterface.edit_resource(load(path))` / `EditorInterface.get_file_system_dock()`
  navigate.

Note: the default word-at-caret won't span a full path with `/`, `.`, `://`. We'll need to
re-extract the token from the line around the click column (we already tokenize RML/RCSS in
`rcss_tokenizer.gd` / the highlighters, so attribute-value extraction is available), then
resolve it.

Plan:
1. Add a small `RmlLinkNavigation` helper attached alongside diagnostics in
   `_ensure_completion_hook`, enabling `symbol_lookup_on_click_enabled` on our buffers.
2. On `symbol_validate`/`symbol_lookup`, detect when the caret is inside a
   `src=`/`href=`/`url(…)`/`@import` value, extract the raw path, resolve it (shared
   resolver below), validate existence, and open it.
3. Resolution must match the **runtime** loader so editor and runtime agree:
   - decode RmlUi's `:`→`|` URL-encoding (the #25 root cause),
   - relative paths join against the current document's directory,
   - absolute `res://`/`user://` paths pass through,
   - default missing scheme to `res://` (mirrors `GodotFileInterface::Open`).

### B. Clickable paths in the Output log (companion, smaller)

Godot's Output panel auto-linkifies text shaped like `res://path:line`. The loader errors
in `GodotScriptDocument::LoadExternalScript` and friends currently print freeform text.
Emitting the resolved `res://…` path (and, where known, `:line`) in that recognized shape
makes them click-to-open for free. The inline-`<script>` path already does line mapping via
`RmlManager::notify_log` with a trailing `: N.` — we'd extend the same idea to external
resource errors.

## Scope / acceptance criteria

- [ ] Ctrl/Cmd-click on a `<script src>` path opens the `.gd` in the script editor.
- [ ] Same for `<link href>` (RCSS), `text/template` href, and RCSS `url(…)`/`@import`.
- [ ] Hovered, resolvable paths are underlined with the hand cursor; unresolvable ones are
      not (no false affordance).
- [ ] Relative and absolute (`res://…`) forms both resolve, identically to runtime.
- [ ] Loader error messages in the Output log are click-to-open where a `res://` path is
      known.
- [ ] No effect on non-RML/RCSS buffers.

## Implementation notes / pointers

- `project/addons/rmlui-godot/editor/rmlui_editor_plugin.gd` — where to attach the new
  click handler (next to completion/diagnostics attach).
- `project/addons/rmlui-godot/editor/rml_diagnostics.gd` — pattern to follow for a
  per-`CodeEdit` attach/detach lifecycle.
- `project/addons/rmlui-godot/editor/rcss_tokenizer.gd` / `rml_syntax_highlighter.gd` —
  reuse for extracting the attribute/url token under the caret.
- `src/rmlui_godot/GodotScriptDocument.cpp` (`LoadExternalScript`) — the canonical runtime
  resolution to mirror; consider factoring the `|`→`:` decode + scheme defaulting into one
  shared helper used by both the file interface and the script loader, so the editor side
  can match it.
- Godot API: `CodeEdit.symbol_lookup_on_click_enabled`, signals `symbol_validate` /
  `symbol_lookup`, `set_symbol_lookup_word_as_valid()`, `EditorInterface.edit_resource()`.

## Relationship to #25

#25 fixes the *runtime* `<script src>` resolution (paths reached the loader still
`|`-encoded). This issue is the *editor* counterpart: now that the paths resolve correctly,
make them navigable. Sharing one resolver keeps the two from drifting apart.
