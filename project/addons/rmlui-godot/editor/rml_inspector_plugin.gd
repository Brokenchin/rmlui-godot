@tool
class_name RmlInspectorPlugin
extends EditorInspectorPlugin
## Inspector header for RmlContext nodes: quick actions (Edit/Create/Reload/
## Open in Preview) and a live status line fed by get_context_info().

const RML_TEMPLATE := """<rml>
<head>
	<title>%s</title>
	<style>
		body {
			font-size: 16dp;
			color: #eeeeee;
		}
	</style>
</head>
<body>
	<h1>%s</h1>
	<p>Edit %s to get started.</p>
</body>
</rml>
"""

## Set by the editor plugin; called with the RmlContext to show in the
## bottom preview panel.
var preview_opener: Callable


func _can_handle(object: Object) -> bool:
	return object is RmlContext


func _parse_begin(object: Object) -> void:
	var ctx := object as RmlContext
	if not ctx:
		return

	var container := VBoxContainer.new()
	var doc_path: String = ctx.get("document_path")
	var doc_exists := not doc_path.is_empty() and FileAccess.file_exists(doc_path)

	# --- Quick action buttons ---
	var btn_row := HBoxContainer.new()

	if doc_exists:
		var btn_edit_rml := Button.new()
		btn_edit_rml.text = "Edit RML"
		btn_edit_rml.tooltip_text = "Open the RML document in the script editor"
		btn_edit_rml.pressed.connect(_on_edit_rml.bind(ctx))
		btn_row.add_child(btn_edit_rml)

		var rcss_paths := RmlPreviewPanel.extract_rcss_links(doc_path)
		var missing_rcss := rcss_paths.size() > 0 \
			and Array(rcss_paths).any(func(p): return not FileAccess.file_exists(p))
		var btn_edit_rcss := Button.new()
		btn_edit_rcss.text = "Create RCSS" if missing_rcss else "Edit RCSS"
		btn_edit_rcss.tooltip_text = "Open the linked .rcss files (creating missing ones)" \
			if missing_rcss else "Open linked .rcss files in the script editor"
		btn_edit_rcss.disabled = rcss_paths.is_empty()
		if rcss_paths.is_empty():
			btn_edit_rcss.tooltip_text = "The document has no <link href=\"*.rcss\"> tags"
		btn_edit_rcss.pressed.connect(_on_edit_rcss.bind(ctx))
		btn_row.add_child(btn_edit_rcss)
	else:
		# document_path empty or pointing at a missing file. The inspector's
		# file picker can only select EXISTING files, so file creation needs
		# its own save dialog.
		var btn_new := Button.new()
		btn_new.text = "New RML..."
		btn_new.tooltip_text = "Create a new .rml document from a starter template,\nassign it to document_path and open it"
		btn_new.pressed.connect(_on_new_rml.bind(ctx))
		btn_row.add_child(btn_new)
		if not doc_path.is_empty():
			var btn_create := Button.new()
			btn_create.text = "Create %s" % doc_path.get_file()
			btn_create.tooltip_text = "Create the missing file %s from a starter template" % doc_path
			btn_create.pressed.connect(_on_create_rml.bind(ctx))
			btn_row.add_child(btn_create)

	var btn_reload := Button.new()
	btn_reload.text = "Reload"
	btn_reload.tooltip_text = "Reload all documents on this context"
	btn_reload.pressed.connect(_on_reload.bind(ctx))
	btn_row.add_child(btn_reload)

	var btn_preview := Button.new()
	btn_preview.text = "Preview"
	btn_preview.tooltip_text = "Show this context in the RmlUI Preview bottom panel"
	btn_preview.disabled = not preview_opener.is_valid()
	btn_preview.pressed.connect(_on_open_preview.bind(ctx))
	btn_row.add_child(btn_preview)

	container.add_child(btn_row)

	# --- Live status line ---
	var status := Label.new()
	status.add_theme_font_size_override("font_size", 12)
	status.modulate = Color(1, 1, 1, 0.7)
	container.add_child(status)

	var timer := Timer.new()
	timer.wait_time = 1.0
	timer.autostart = true
	timer.timeout.connect(_update_status.bind(ctx, status))
	status.add_child(timer)
	_update_status(ctx, status)

	add_custom_control(container)


func _update_status(ctx: RmlContext, status: Label) -> void:
	if not is_instance_valid(ctx) or not is_instance_valid(status):
		return
	if not ctx.has_method("get_context_info"):
		status.text = ""
		return
	var info: Dictionary = ctx.get_context_info()
	if not info.get("initialized", false):
		status.text = "Context not initialized"
		return
	status.text = "%dx%d · %d docs · %d models · %d listeners · %d batches · %d textures" % [
		info.get("width", 0), info.get("height", 0),
		info.get("num_documents", 0), info.get("num_data_models", 0),
		info.get("num_listeners", 0), info.get("num_geometry", 0),
		info.get("num_textures", 0),
	]


func _on_edit_rml(ctx: RmlContext) -> void:
	var path: String = ctx.get("document_path")
	if path.is_empty():
		return
	_open_in_editor(path, ctx)


func _on_edit_rcss(ctx: RmlContext) -> void:
	var rml_path: String = ctx.get("document_path")
	if rml_path.is_empty():
		return
	for rcss_path in RmlPreviewPanel.extract_rcss_links(rml_path):
		if not FileAccess.file_exists(rcss_path):
			_create_text_file(rcss_path, "/* %s */\n\nbody {\n}\n" % rcss_path.get_file())
		_open_in_editor(rcss_path, ctx)


func _on_new_rml(ctx: RmlContext) -> void:
	var dialog := EditorFileDialog.new()
	dialog.file_mode = EditorFileDialog.FILE_MODE_SAVE_FILE
	dialog.access = EditorFileDialog.ACCESS_RESOURCES
	dialog.add_filter("*.rml", "RmlUi Document")
	dialog.title = "New RML Document"
	dialog.current_file = "new_document.rml"
	dialog.file_selected.connect(func(path: String):
		if not path.get_extension():
			path += ".rml"
		var title := path.get_file().get_basename().capitalize()
		if FileAccess.file_exists(path) \
			or _create_text_file(path, RML_TEMPLATE % [title, title, path.get_file()]):
			if is_instance_valid(ctx):
				ctx.set("document_path", path)  # setter loads live + refreshes inspector
			_open_in_editor(path, ctx)
		dialog.queue_free())
	dialog.canceled.connect(dialog.queue_free)
	EditorInterface.get_base_control().add_child(dialog)
	dialog.popup_centered_ratio(0.5)


func _on_create_rml(ctx: RmlContext) -> void:
	var path: String = ctx.get("document_path")
	if path.is_empty() or FileAccess.file_exists(path):
		return
	var title := path.get_file().get_basename().capitalize()
	if not _create_text_file(path, RML_TEMPLATE % [title, title, path.get_file()]):
		return
	if ctx.has_method("load_document"):
		ctx.load_document(path)  # the _ready load failed while the file was missing
	_open_in_editor(path, ctx)
	# Rebuild the inspector so the buttons switch to Edit RML / Edit RCSS.
	ctx.notify_property_list_changed()


func _on_reload(ctx: RmlContext) -> void:
	if ctx.has_method("reload_all_documents"):
		ctx.reload_all_documents()


func _on_open_preview(ctx: RmlContext) -> void:
	if preview_opener.is_valid():
		preview_opener.call(ctx)


func _create_text_file(path: String, content: String) -> bool:
	var dir := path.get_base_dir()
	if not DirAccess.dir_exists_absolute(dir):
		DirAccess.make_dir_recursive_absolute(dir)
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		push_warning("RmlUI: Cannot create file: %s" % path)
		return false
	f.store_string(content)
	f.close()
	EditorInterface.get_resource_filesystem().scan()
	return true


## Instance wrapper: opens the file via the shared opener (which handles text
## files through the FileSystem-dock double-click — no public open-text-file
## API exists), then restores the inspector to the context node, since opening
## can push the file into the Inspector but from these buttons the user is
## working ON the node.
func _open_in_editor(path: String, refocus: Node = null) -> void:
	RmlEditorOpen.open_file(path)
	if refocus and is_instance_valid(refocus):
		EditorInterface.edit_node.call_deferred(refocus)
