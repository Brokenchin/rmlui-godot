@tool
class_name RmlLinkNavigation
extends Node
## Ctrl/Cmd-click navigation for .rml/.rcss buffers in the script editor
## (issue #23). Hovering a path inside src="…", href="…", url(…) or @import
## with Ctrl held underlines it (the same affordance GDScript uses for "jump to
## definition"); clicking opens the referenced file.
##
## Path resolution mirrors the runtime loader so editor and runtime agree on
## what a path points at (see GodotScriptDocument::LoadExternalScript and the
## file interface):
##   - decode RmlUi's ':'→'|' URL-encoding (parity; editor text is rarely
##     encoded, but harmless),
##   - relative paths join against the edited document's directory,
##   - absolute res://, user:// pass through,
##   - a leading '/' or a bare path defaults to res://.
##
## The script editor exposes no API for the edited text file's path (issue #23
## investigation: ScriptEditorBase has only get_base_editor()), so the plugin
## passes the path read from the tab tooltip. When that's unavailable, relative
## paths fall back to a project-wide suffix search so navigation still works.
##
## Opening: .gd and other loadable resources open directly in their editor;
## .rml/.rcss are plain text files with no public "open text file" API
## (ScriptEditor exposes only close_file), so they are revealed + selected in
## the FileSystem dock — the closest navigable affordance the public API allows.

var _ce: CodeEdit
var _doc_path := ""  # res:// path of the file in _ce, for relative resolution
var _files_cache: PackedStringArray = []  # lazily-built project file index


func _exit_tree() -> void:
	detach()


## Called by the editor plugin's poll when the active tab changes. `doc_path`
## is the edited file's res:// path (for relative link resolution); pass "" for
## buffers that aren't ours, which detaches.
func attach(ce: CodeEdit, doc_path: String) -> void:
	if ce == _ce:
		_doc_path = doc_path
		return
	detach()
	if ce == null:
		return
	_ce = ce
	_doc_path = doc_path
	ce.symbol_lookup_on_click = true
	if not ce.symbol_validate.is_connected(_on_symbol_validate):
		ce.symbol_validate.connect(_on_symbol_validate)
	if not ce.symbol_lookup.is_connected(_on_symbol_lookup):
		ce.symbol_lookup.connect(_on_symbol_lookup)


func detach() -> void:
	if _ce and is_instance_valid(_ce):
		if _ce.symbol_validate.is_connected(_on_symbol_validate):
			_ce.symbol_validate.disconnect(_on_symbol_validate)
		if _ce.symbol_lookup.is_connected(_on_symbol_lookup):
			_ce.symbol_lookup.disconnect(_on_symbol_lookup)
	_ce = null
	_doc_path = ""


# --- Signal handlers ---

## Ctrl-hover: decide whether to underline the hovered text. The signal only
## carries the bare word (paths span several words), so re-derive the click
## position from the mouse and extract the full path token there.
func _on_symbol_validate(_symbol: String) -> void:
	if _ce == null or not is_instance_valid(_ce):
		return
	var lc := _ce.get_line_column_at_pos(_ce.get_local_mouse_position())
	var target := _target_at(lc.y, lc.x)
	_ce.set_symbol_lookup_word_as_valid(not target.is_empty())


## Ctrl-click: the signal carries line/column, so extract and open directly.
func _on_symbol_lookup(_symbol: String, line: int, column: int) -> void:
	if _ce == null or not is_instance_valid(_ce):
		return
	var target := _target_at(line, column)
	if not target.is_empty():
		_open(target)


## Existing file the path token at (line, col) resolves to, or "" when the caret
## isn't over a resolvable link path.
func _target_at(line: int, col: int) -> String:
	if line < 0 or line >= _ce.get_line_count():
		return ""
	return _resolve(path_token_at(_ce.get_line(line), col))


## Resolve a raw path to an existing project file ("" if none). Tries the exact
## doc-relative resolution first, then a project-wide suffix search so relative
## links still resolve when the document's own path is unknown.
func _resolve(raw: String) -> String:
	if raw.strip_edges().is_empty():
		return ""
	var direct := resolve_path(raw, _doc_path)
	if not direct.is_empty() and FileAccess.file_exists(direct):
		return direct
	return best_suffix_match(_all_project_files(), raw)


func _open(path: String) -> void:
	# Shared opener handles scripts/scenes/resources directly and text files via
	# the FileSystem-dock double-click (no public open-text-file API exists).
	RmlEditorOpen.open_file(path)


## All project file paths under res://, built once and cached. Used only when
## exact resolution fails, so the walk cost is paid rarely.
func _all_project_files() -> PackedStringArray:
	if _files_cache.is_empty():
		_files_cache = _walk("res://")
	return _files_cache


func _walk(dir: String) -> PackedStringArray:
	var out := PackedStringArray()
	var da := DirAccess.open(dir)
	if da == null:
		return out
	da.list_dir_begin()
	var entry := da.get_next()
	while entry != "":
		if not entry.begins_with("."):  # skip .godot, .git, hidden
			var p := dir.path_join(entry)
			if da.current_is_dir():
				out.append_array(_walk(p))
			else:
				out.append(p)
		entry = da.get_next()
	da.list_dir_end()
	return out


# --- Pure helpers (unit-tested in test_link_navigation.gd) ---

## The path string of the src=/href=/url()/@import value spanning column `col`
## on `line`, or "" when `col` isn't inside such a value.
static func path_token_at(line: String, col: int) -> String:
	# Each pattern captures the path in group 1. Order is irrelevant: a column
	# lands inside at most one value span. url(...) accepts quoted or bare.
	const PATTERNS := [
		"(?:href|src)\\s*=\\s*\"([^\"]*)\"",
		"(?:href|src)\\s*=\\s*'([^']*)'",
		"@import\\s+\"([^\"]*)\"",
		"@import\\s+'([^']*)'",
		"url\\(\\s*\"([^\"]*)\"\\s*\\)",
		"url\\(\\s*'([^']*)'\\s*\\)",
		"url\\(\\s*([^\"')]+?)\\s*\\)",
	]
	for p in PATTERNS:
		var re := RegEx.create_from_string(p)
		for m in re.search_all(line):
			# Inclusive bounds so the caret at either edge of the value counts.
			if m.get_start(1) <= col and col <= m.get_end(1):
				return m.get_string(1).strip_edges()
	return ""


## Resolve a raw link path against the edited document, matching runtime rules.
## Returns "" for an empty input.
static func resolve_path(raw: String, doc_path: String) -> String:
	if raw.strip_edges().is_empty():
		return ""
	var p := raw.strip_edges().replace("|", ":")
	if p.begins_with("res://") or p.begins_with("user://"):
		return p.simplify_path()
	if p.begins_with("/"):
		return ("res://" + p.substr(1)).simplify_path()
	var base := doc_path.get_base_dir()
	if base.is_empty():
		base = "res://"
	return base.path_join(p).simplify_path()


## Pick the project file that best matches a raw link path by its tail: prefer a
## file whose path ends with the path's full relative tail (e.g. ".../theme/
## theme.rcss"), else the first one sharing its basename. Returns "" for none.
static func best_suffix_match(files: PackedStringArray, raw: String) -> String:
	var norm := raw.strip_edges().replace("|", ":")
	norm = norm.trim_prefix("res://").trim_prefix("user://")
	while norm.begins_with("../") or norm.begins_with("./") or norm.begins_with("/"):
		norm = norm.trim_prefix("../").trim_prefix("./").trim_prefix("/")
	if norm.is_empty():
		return ""
	var base := norm.get_file()
	var by_base := ""
	for f in files:
		if f.ends_with("/" + norm):
			return f  # strongest: full relative tail matches
		if by_base.is_empty() and f.get_file() == base:
			by_base = f
	return by_base
