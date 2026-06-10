@tool
class_name RmlDiagnostics
extends Node
## GDScript-style inline diagnostics for .rml/.rcss buffers in the script
## editor: red highlight on offending lines + an error bar under the editor.
##
## Validation runs through a hidden throwaway RmlContext, so it works with no
## RmlContext selected and never touches scene state. Only RmlUi's internal
## parser messages reach RmlManager.get_recent_log() (the plugin's own
## push_warning calls don't go through notify_log), and the log is cleared
## before each validation — so every captured entry belongs to this buffer.

const DEBOUNCE := 0.5
const ALIAS := "diag://buffer"
const LINE_TINT_ERROR := Color(0.75, 0.22, 0.22, 0.35)
const LINE_TINT_WARNING := Color(0.85, 0.65, 0.2, 0.22)
const CLEAR_TINT := Color(0, 0, 0, 0)

var _ce: CodeEdit
var _kind := ""  # "rml" | "rcss"
var _hidden_ctx: Node
var _doc_loaded := false
var _debounce: Timer
var _error_bar: PanelContainer
var _error_label: Label
var _painted_lines: PackedInt32Array = []
var _retries := 0

const MAX_RETRIES := 100


func _ensure_debounce() -> Timer:
	# Created lazily — attach() may run before this node's _ready.
	if _debounce == null or not is_instance_valid(_debounce):
		_debounce = Timer.new()
		_debounce.wait_time = DEBOUNCE
		_debounce.one_shot = true
		_debounce.timeout.connect(_validate)
		add_child(_debounce)
	return _debounce


func _exit_tree() -> void:
	detach()


## Called by the editor plugin's poll whenever the current script-editor tab
## changes. `kind` is "" for buffers that aren't ours.
func attach(ce: CodeEdit, kind: String) -> void:
	if ce == _ce:
		return
	detach()
	if ce == null or kind.is_empty():
		return
	_ce = ce
	_kind = kind
	ce.text_changed.connect(_on_text_changed)
	_ensure_error_label()
	_retries = 0
	_schedule_validate()


func detach() -> void:
	if _ce and is_instance_valid(_ce):
		if _ce.text_changed.is_connected(_on_text_changed):
			_ce.text_changed.disconnect(_on_text_changed)
		_clear_paint()
	if _error_bar and is_instance_valid(_error_bar):
		_error_bar.queue_free()
	_error_bar = null
	_error_label = null
	_ce = null
	_kind = ""


func _on_text_changed() -> void:
	_retries = 0
	_schedule_validate()


func _schedule_validate() -> void:
	if is_inside_tree():
		_ensure_debounce().start()
	else:
		# Not in the tree yet (timers can't run) — defer to next idle frame.
		call_deferred("_validate")


# --- Validation ---

func _validate() -> void:
	if _ce == null or not is_instance_valid(_ce):
		return
	var mgr: Object = Engine.get_singleton("RmlManager") if Engine.has_singleton("RmlManager") else null
	if mgr == null or not _ensure_hidden_context():
		return

	# A freshly created context may not have run _ready yet — retry shortly.
	var info: Dictionary = _hidden_ctx.call("get_context_info")
	if not info.get("initialized", false):
		_retries += 1
		if _retries <= MAX_RETRIES:
			_schedule_validate()
		return

	# Unload the previous validation document BEFORE clearing the log, so a
	# stale-unload warning can never be mistaken for a buffer error.
	if _doc_loaded:
		_hidden_ctx.call("unload_document", ALIAS)
		_doc_loaded = false

	mgr.clear_recent_log()

	var text := _ce.text
	if _kind == "rcss":
		_hidden_ctx.call("inject_stylesheet", text)
	else:
		_doc_loaded = _hidden_ctx.call("load_document_from_string", text, ALIAS)

	_apply_diagnostics(_parse_log(mgr.get_recent_log()))


func _ensure_hidden_context() -> bool:
	if _hidden_ctx and is_instance_valid(_hidden_ctx):
		return true
	if not ClassDB.class_exists(&"RmlContext"):
		return false
	_hidden_ctx = ClassDB.instantiate(&"RmlContext")
	if _hidden_ctx == null:
		return false
	_hidden_ctx.name = "DiagnosticsContext"
	_hidden_ctx.visible = false
	add_child(_hidden_ctx)
	return true


# --- Log parsing ---

## RmlUi line info formats (all 1-based):
##   "... in <file>: <line>."     (property declaration errors)
##   "... at <file>:<line>"       (end-of-rule errors)
##   "XML parse error on line <line> of <file>."
func _parse_log(entries: Array) -> Array:
	var diags: Array = []
	var re_xml := RegEx.create_from_string("on line (\\d+) of")
	var re_tail := RegEx.create_from_string(":\\s*(\\d+)\\.?\\s*$")
	for entry in entries:
		var level: int = entry.get("level", 4)
		if level > 3:
			continue
		var msg: String = entry.get("message", "")
		# Environment noise, not buffer errors: the hidden validation context
		# has no fonts of its own (font loading is per-scene configuration).
		if msg.begins_with("No font face defined"):
			continue
		var line := -1
		var m := re_xml.search(msg)
		if m == null:
			m = re_tail.search(msg)
		if m:
			line = m.get_string(1).to_int() - 1
		# RmlUi logs most parse problems as warnings (LT_WARNING=3) — classify
		# by message so syntax/XML errors paint red like GDScript errors.
		var is_err: bool = level <= 2 \
			or msg.containsn("syntax error") \
			or msg.containsn("parse error") \
			or msg.containsn("failed")
		diags.append({"line": line, "message": msg, "is_error": is_err})
	return diags


# --- Painting ---

func _apply_diagnostics(diags: Array) -> void:
	_clear_paint()
	if _ce == null or not is_instance_valid(_ce):
		return

	if diags.is_empty():
		if _error_bar and is_instance_valid(_error_bar):
			_error_bar.visible = false
		return

	var line_count := _ce.get_line_count()
	for d in diags:
		var line: int = d.line
		if line < 0 or line >= line_count:
			continue
		_ce.set_line_background_color(line, LINE_TINT_ERROR if d.is_error else LINE_TINT_WARNING)
		_painted_lines.append(line)

	if _error_label and is_instance_valid(_error_label):
		var first: Dictionary = diags[0]
		var loc := "Line %d: " % (first.line + 1) if first.line >= 0 else ""
		var suffix := "  (+%d more)" % (diags.size() - 1) if diags.size() > 1 else ""
		_error_label.text = loc + first.message + suffix
		_error_label.tooltip_text = "\n".join(diags.map(func(d): return d.message))
		_error_label.add_theme_color_override("font_color",
			Color(0.95, 0.55, 0.55) if first.is_error else Color(0.95, 0.85, 0.5))
		_error_bar.visible = true


func _clear_paint() -> void:
	if _ce and is_instance_valid(_ce):
		var line_count := _ce.get_line_count()
		for line in _painted_lines:
			if line < line_count:
				_ce.set_line_background_color(line, CLEAR_TINT)
	_painted_lines.clear()


func _ensure_error_label() -> void:
	if _error_bar and is_instance_valid(_error_bar):
		return
	if _ce == null or not is_instance_valid(_ce):
		return
	# The script editor wraps the CodeEdit in a container (CodeTextEditor-like
	# VBox); appending there places the bar under the editor, matching the
	# GDScript error bar. If the layout ever changes, the line highlights
	# still work without the bar. PanelContainer gives the bar a background
	# and a real minimum size — a bare Label can get squeezed to nothing by
	# the editor's internal layout.
	var parent := _ce.get_parent()
	if parent == null:
		return

	_error_bar = PanelContainer.new()
	_error_bar.visible = false
	_error_bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.14, 0.11, 0.12)
	style.content_margin_left = 8.0
	style.content_margin_right = 8.0
	style.content_margin_top = 3.0
	style.content_margin_bottom = 3.0
	_error_bar.add_theme_stylebox_override("panel", style)

	_error_label = Label.new()
	_error_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_error_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_error_label.custom_minimum_size = Vector2(0, 18)
	_error_bar.add_child(_error_label)
	parent.add_child(_error_bar)
