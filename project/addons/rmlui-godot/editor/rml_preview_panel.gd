@tool
class_name RmlPreviewPanel
extends VBoxContainer

enum Background { TRANSPARENT, DARK, LIGHT }

var _tracked_context: Node
var _preview_context: Node
var _viewport: SubViewport
var _viewport_container: SubViewportContainer
var _preview_bg: ColorRect
var _no_preview_label: Label
var _file_label: Label
var _info_label: Label
var _watch_timer: Timer
var _watched_files: Dictionary = {}

func _ready() -> void:
	custom_minimum_size = Vector2(0, 200)
	_build_toolbar()
	_build_status_bar()
	_build_preview_area()
	_build_file_watcher()

func _build_toolbar() -> void:
	var toolbar := HBoxContainer.new()
	toolbar.add_theme_constant_override("separation", 6)

	var reload_btn := Button.new()
	reload_btn.text = "Reload"
	reload_btn.tooltip_text = "Reload all documents in the preview"
	reload_btn.pressed.connect(_reload_preview)
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

func _build_file_watcher() -> void:
	_watch_timer = Timer.new()
	_watch_timer.wait_time = 1.0
	_watch_timer.timeout.connect(_poll_file_changes)
	add_child(_watch_timer)

func _exit_tree() -> void:
	_clear_preview()
	_watched_files.clear()
	if _watch_timer:
		_watch_timer.stop()

# --- Public ---

func track_context(ctx: Node) -> void:
	if _tracked_context == ctx:
		return
	_tracked_context = ctx
	_watched_files.clear()
	_watch_timer.stop()
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
		_file_label.text = "Selected context has no document_path"
		_clear_preview()
		return

	if not _ensure_preview_context():
		_file_label.text = "Cannot create preview (GDExtension not loaded?)"
		return

	var font_paths: PackedStringArray = _tracked_context.get("font_paths")
	for fp in font_paths:
		_preview_context.call("load_font_face", fp)

	_preview_context.call("load_document", doc_path)

	_watch_file(doc_path)
	for rcss in extract_rcss_links(doc_path):
		_watch_file(rcss)
	_watch_timer.start()

	_file_label.text = doc_path.get_file()
	_no_preview_label.visible = false
	_viewport_container.visible = true
	_update_info()

func _clear_preview() -> void:
	if _preview_context and is_instance_valid(_preview_context):
		_preview_context.queue_free()
		_preview_context = null
	_viewport_container.visible = false
	_no_preview_label.visible = true
	_file_label.text = "No document"
	_info_label.text = ""

func _ensure_preview_context() -> bool:
	if _preview_context and is_instance_valid(_preview_context):
		return true
	if not ClassDB.class_exists(&"RmlContext"):
		return false
	_preview_context = ClassDB.instantiate(&"RmlContext")
	if not _preview_context:
		return false
	_preview_context.name = "EditorPreview"
	_viewport.add_child(_preview_context)
	_preview_context.set_anchors_preset(Control.PRESET_FULL_RECT)
	return true

func _reload_preview() -> void:
	if not _tracked_context or not is_instance_valid(_tracked_context):
		return
	if _preview_context and is_instance_valid(_preview_context):
		_preview_context.queue_free()
		_preview_context = null
	_load_from_context()

func _update_info() -> void:
	if not _preview_context or not _preview_context.has_method("get_context_info"):
		_info_label.text = ""
		return
	var info: Dictionary = _preview_context.call("get_context_info")
	var docs: int = info.get("num_documents", 0)
	var geom: int = info.get("num_geometry", 0)
	_info_label.text = "%d docs · %d batches" % [docs, geom]

# --- File watching ---

func _watch_file(path: String) -> void:
	if not FileAccess.file_exists(path):
		return
	_watched_files[path] = FileAccess.get_modified_time(path)

func _poll_file_changes() -> void:
	if not is_instance_valid(_tracked_context):
		_clear_preview()
		_watch_timer.stop()
		return

	var changed := false
	for path in _watched_files:
		if not FileAccess.file_exists(path):
			continue
		var mtime := FileAccess.get_modified_time(path)
		if mtime != _watched_files[path]:
			_watched_files[path] = mtime
			changed = true
	if changed:
		_reload_preview()

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
