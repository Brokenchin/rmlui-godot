@tool
class_name RmlPreviewPanel
extends VBoxContainer
## Bottom-panel live preview for RmlContext documents.
##
## Tracks the selected RmlContext, renders its document in an isolated preview
## context, and live-applies edits from the script editor without saving:
## - .rcss buffers are injected on top of the document's saved stylesheets
## - .rml buffers replace the document entirely (load_document_from_string)
## Edited buffers are identified by their syntax highlighter, so live editing
## only reacts to .rml/.rcss tabs. Saved-file changes are picked up by a 1s
## mtime poll, which also resets live overrides (disk is truth after save).

enum Background { TRANSPARENT, DARK, LIGHT }

const LIVE_EDIT_DEBOUNCE := 0.35  # seconds after last keystroke

var _tracked_context: Node
var _preview_context: Node
var _viewport: SubViewport
var _viewport_container: SubViewportContainer
var _preview_bg: ColorRect
var _no_preview_label: Label
var _file_label: Label
var _info_label: Label
var _error_label: Label
var _watch_timer: Timer
var _debounce_timer: Timer
var _watched_files: Dictionary = {}
var _connected_editor: TextEdit
var _live_rml_text := ""
var _live_rcss_text := ""

func _ready() -> void:
	custom_minimum_size = Vector2(0, 200)
	_build_toolbar()
	_build_status_bar()
	_build_preview_area()
	_build_timers()
	_connect_rml_log()

func _build_toolbar() -> void:
	var toolbar := HBoxContainer.new()
	toolbar.add_theme_constant_override("separation", 6)

	var reload_btn := Button.new()
	reload_btn.text = "Reload"
	reload_btn.tooltip_text = "Discard live edits and reload from saved files"
	reload_btn.pressed.connect(_on_reload_pressed)
	toolbar.add_child(reload_btn)

	toolbar.add_child(VSeparator.new())

	var dp_label := Label.new()
	dp_label.text = "DPI:"
	toolbar.add_child(dp_label)

	var dp_slider := HSlider.new()
	dp_slider.min_value = 0.5
	dp_slider.max_value = 3.0
	dp_slider.step = 0.25
	dp_slider.value = 1.0
	dp_slider.custom_minimum_size.x = 80
	dp_slider.tooltip_text = "dp_ratio — scales density-independent units"
	dp_slider.value_changed.connect(_on_dp_ratio_changed)
	toolbar.add_child(dp_slider)

	toolbar.add_child(VSeparator.new())

	var bg_label := Label.new()
	bg_label.text = "BG:"
	toolbar.add_child(bg_label)

	var bg_opt := OptionButton.new()
	bg_opt.add_item("Transparent")
	bg_opt.add_item("Dark")
	bg_opt.add_item("Light")
	bg_opt.select(Background.DARK)
	bg_opt.item_selected.connect(_on_bg_changed)
	toolbar.add_child(bg_opt)

	add_child(toolbar)

func _build_status_bar() -> void:
	var bar := HBoxContainer.new()
	_file_label = Label.new()
	_file_label.text = "No document"
	_file_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_file_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	bar.add_child(_file_label)
	_info_label = Label.new()
	_info_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	bar.add_child(_info_label)
	add_child(bar)

	_error_label = Label.new()
	_error_label.visible = false
	_error_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_error_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_error_label.max_lines_visible = 2
	add_child(_error_label)

func _build_preview_area() -> void:
	var panel := PanelContainer.new()
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL

	_no_preview_label = Label.new()
	_no_preview_label.text = "Select an RmlContext node to preview its document"
	_no_preview_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_no_preview_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_no_preview_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_no_preview_label.size_flags_vertical = Control.SIZE_EXPAND_FILL
	panel.add_child(_no_preview_label)

	_viewport_container = SubViewportContainer.new()
	_viewport_container.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_viewport_container.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_viewport_container.stretch = true
	_viewport_container.visible = false

	_viewport = SubViewport.new()
	_viewport.transparent_bg = true
	_viewport.render_target_update_mode = SubViewport.UPDATE_WHEN_PARENT_VISIBLE

	_preview_bg = ColorRect.new()
	_preview_bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	_preview_bg.color = Color(0.12, 0.12, 0.14)
	_viewport.add_child(_preview_bg)

	_viewport_container.add_child(_viewport)
	panel.add_child(_viewport_container)
	add_child(panel)

func _build_timers() -> void:
	_watch_timer = Timer.new()
	_watch_timer.wait_time = 1.0
	_watch_timer.timeout.connect(_on_watch_tick)
	add_child(_watch_timer)

	_debounce_timer = Timer.new()
	_debounce_timer.wait_time = LIVE_EDIT_DEBOUNCE
	_debounce_timer.one_shot = true
	_debounce_timer.timeout.connect(_apply_live_edit)
	add_child(_debounce_timer)

func _connect_rml_log() -> void:
	if not Engine.has_singleton("RmlManager"):
		return
	var mgr := Engine.get_singleton("RmlManager")
	if mgr.has_signal("rml_log") and not mgr.is_connected("rml_log", _on_rml_log):
		mgr.connect("rml_log", _on_rml_log)

func _exit_tree() -> void:
	_clear_preview()
	_watched_files.clear()
	_disconnect_editor()
	if _watch_timer:
		_watch_timer.stop()

# --- Public ---

func track_context(ctx: Node) -> void:
	if _tracked_context == ctx:
		return
	_tracked_context = ctx
	_watched_files.clear()
	_watch_timer.stop()
	_live_rml_text = ""
	_live_rcss_text = ""
	if ctx:
		_load_from_context()
	else:
		_clear_preview()

# --- Preview lifecycle ---

func _load_from_context() -> void:
	if not is_instance_valid(_tracked_context):
		_clear_preview()
		return

	var doc_path: String = _tracked_context.get("document_path")
	if doc_path.is_empty():
		_clear_preview()
		_file_label.text = "No document_path on '%s'" % _tracked_context.name
		_no_preview_label.text = (
			"RmlContext '%s' has no document_path.\n" % _tracked_context.name
			+ "Set Auto-Configuration → Document Path in the inspector to enable the preview.\n"
			+ "(Script-driven load_document() calls don't run in the editor.)"
		)
		return

	if not _ensure_preview_context():
		_file_label.text = "Cannot create preview (GDExtension not loaded?)"
		return

	_clear_error()
	var mgr: Object = Engine.get_singleton("RmlManager") if Engine.has_singleton("RmlManager") else null
	if mgr and mgr.has_method("clear_recent_log"):
		mgr.clear_recent_log()

	var font_paths: PackedStringArray = _tracked_context.get("font_paths")
	for fp in font_paths:
		_preview_context.call("load_font_face", fp)

	_apply_mock_data()

	# Live RML override replaces the document; live RCSS injects on top.
	# alias_path = the real path so relative <link href> still resolves.
	if not _live_rml_text.is_empty() and _preview_context.has_method("load_document_from_string"):
		_preview_context.call("load_document_from_string", _live_rml_text, doc_path)
	else:
		_preview_context.call("load_document", doc_path)

	if not _live_rcss_text.is_empty():
		_preview_context.call("inject_stylesheet", _live_rcss_text)

	_watch_file(doc_path)
	for rcss in extract_rcss_links(doc_path):
		_watch_file(rcss)
	_watch_timer.start()

	# Surface any parse errors/warnings the loads just produced. The rml_log
	# signal covers async cases; this pull is the deterministic path.
	if mgr and mgr.has_method("get_recent_log"):
		for entry in mgr.get_recent_log():
			var lvl: int = entry.get("level", 4)
			if lvl <= 3:
				_on_rml_log(lvl, entry.get("message", ""))

	var live_tag := ""
	if not _live_rml_text.is_empty() or not _live_rcss_text.is_empty():
		live_tag = "  (live)"
	_file_label.text = doc_path.get_file() + live_tag
	_no_preview_label.visible = false
	_viewport_container.visible = true
	_update_info()

func _clear_preview() -> void:
	if _preview_context and is_instance_valid(_preview_context):
		_preview_context.queue_free()
		_preview_context = null
	_viewport_container.visible = false
	_no_preview_label.visible = true
	_no_preview_label.text = "Select an RmlContext node to preview its document"
	_file_label.text = "No document"
	_info_label.text = ""
	_clear_error()

func _ensure_preview_context() -> bool:
	# Always rebuilt from scratch — cheapest way to guarantee reload semantics.
	if _preview_context and is_instance_valid(_preview_context):
		_preview_context.queue_free()
		_preview_context = null
	if not ClassDB.class_exists(&"RmlContext"):
		return false
	_preview_context = ClassDB.instantiate(&"RmlContext")
	if not _preview_context:
		return false
	_preview_context.name = "EditorPreview"
	_viewport.add_child(_preview_context)
	_preview_context.set_anchors_preset(Control.PRESET_FULL_RECT)
	# A viewport only lays out children when ITS size changes — a Control
	# added to an already-sized SubViewport stays 0x0 forever otherwise,
	# and a 0x0 context CPU-culls all of its geometry.
	_preview_context.size = _viewport.size
	return true

func _on_reload_pressed() -> void:
	_live_rml_text = ""
	_live_rcss_text = ""
	_reload_preview()

func _reload_preview() -> void:
	if not _tracked_context or not is_instance_valid(_tracked_context):
		return
	_load_from_context()

func _update_info() -> void:
	if not _preview_context or not _preview_context.has_method("get_context_info"):
		_info_label.text = ""
		return
	var info: Dictionary = _preview_context.call("get_context_info")
	var docs: int = info.get("num_documents", 0)
	var geom: int = info.get("num_geometry", 0)
	_info_label.text = "%d docs · %d batches" % [docs, geom]

# --- Mock data (editor_mock_data: {model_name: {var_name: value}}) ---

func _apply_mock_data() -> void:
	if not is_instance_valid(_tracked_context):
		return
	var mock = _tracked_context.get("editor_mock_data")
	if mock == null or not mock is Dictionary or mock.is_empty():
		return
	for model_name in mock:
		var vars = mock[model_name]
		if not vars is Dictionary:
			push_warning("RmlUI preview: editor_mock_data[%s] must be a Dictionary" % model_name)
			continue
		if not _preview_context.call("create_data_model", model_name):
			continue
		for var_name in vars:
			var value = vars[var_name]
			if value is Array:
				_preview_context.call("bind_data_array", model_name, var_name, value)
			else:
				_preview_context.call("bind_data_variable", model_name, var_name, value)

# --- Live editing ---

func _on_watch_tick() -> void:
	if not is_instance_valid(_tracked_context):
		_clear_preview()
		_watch_timer.stop()
		return
	_ensure_editor_connection()
	_poll_file_changes()

func _ensure_editor_connection() -> void:
	var ed := EditorInterface.get_script_editor().get_current_editor()
	if ed == null:
		return
	var te := ed.get_base_editor() as TextEdit
	if te == null or te == _connected_editor:
		return
	_disconnect_editor()
	_connected_editor = te
	te.text_changed.connect(_on_editor_text_changed)

func _disconnect_editor() -> void:
	if _connected_editor and is_instance_valid(_connected_editor):
		if _connected_editor.text_changed.is_connected(_on_editor_text_changed):
			_connected_editor.text_changed.disconnect(_on_editor_text_changed)
	_connected_editor = null

func _on_editor_text_changed() -> void:
	if not is_instance_valid(_tracked_context):
		return
	# Only react to .rml/.rcss buffers — identified by their highlighter.
	var hl := _connected_editor.syntax_highlighter if is_instance_valid(_connected_editor) else null
	if hl is RmlSyntaxHighlighter or hl is RcssSyntaxHighlighter:
		_debounce_timer.start()

func _apply_live_edit() -> void:
	if not is_instance_valid(_connected_editor) or not is_instance_valid(_tracked_context):
		return
	var hl := _connected_editor.syntax_highlighter
	if hl is RmlSyntaxHighlighter:
		_live_rml_text = _connected_editor.text
	elif hl is RcssSyntaxHighlighter:
		_live_rcss_text = _connected_editor.text
	else:
		return
	_reload_preview()

# --- File watching ---

func _watch_file(path: String) -> void:
	if not FileAccess.file_exists(path):
		return
	_watched_files[path] = FileAccess.get_modified_time(path)

func _poll_file_changes() -> void:
	var changed := false
	for path in _watched_files:
		if not FileAccess.file_exists(path):
			continue
		var mtime := FileAccess.get_modified_time(path)
		if mtime != _watched_files[path]:
			_watched_files[path] = mtime
			changed = true
	if changed:
		# Saved files are the source of truth again — drop live overrides.
		_live_rml_text = ""
		_live_rcss_text = ""
		_reload_preview()

# --- Error surface ---

func _on_rml_log(level: int, message: String) -> void:
	# Rml::Log::Type: 1=error, 2=assert, 3=warning. Ignore info/debug.
	if level > 3:
		return
	_error_label.text = message
	_error_label.add_theme_color_override("font_color",
		Color(0.95, 0.55, 0.55) if level <= 2 else Color(0.95, 0.85, 0.5))
	_error_label.visible = true

func _clear_error() -> void:
	if _error_label:
		_error_label.text = ""
		_error_label.visible = false

# --- Toolbar callbacks ---

func _on_dp_ratio_changed(value: float) -> void:
	if _preview_context and is_instance_valid(_preview_context):
		_preview_context.set("dp_ratio", value)

func _on_bg_changed(idx: int) -> void:
	match idx:
		Background.TRANSPARENT:
			_preview_bg.color = Color.TRANSPARENT
		Background.DARK:
			_preview_bg.color = Color(0.12, 0.12, 0.14)
		Background.LIGHT:
			_preview_bg.color = Color(0.9, 0.9, 0.92)

# --- Helpers ---

static func extract_rcss_links(rml_path: String) -> PackedStringArray:
	var result := PackedStringArray()
	if not FileAccess.file_exists(rml_path):
		return result
	var content := FileAccess.get_file_as_string(rml_path)
	var base_dir := rml_path.get_base_dir()
	var regex := RegEx.new()
	regex.compile('href="([^"]+\\.rcss)"')
	for m in regex.search_all(content):
		var href: String = m.get_string(1)
		if href.begins_with("res://"):
			result.append(href)
		else:
			result.append(base_dir.path_join(href))
	return result
