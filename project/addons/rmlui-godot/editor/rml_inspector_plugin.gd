@tool
class_name RmlInspectorPlugin
extends EditorInspectorPlugin

func _can_handle(object: Object) -> bool:
	return object is RmlContext

func _parse_begin(object: Object) -> void:
	var ctx := object as RmlContext
	if not ctx:
		return

	var container := VBoxContainer.new()

	# Quick action buttons
	var btn_row := HBoxContainer.new()

	var btn_edit_rml := Button.new()
	btn_edit_rml.text = "Edit RML"
	btn_edit_rml.tooltip_text = "Open the RML document in the script editor"
	btn_edit_rml.pressed.connect(_on_edit_rml.bind(ctx))
	btn_row.add_child(btn_edit_rml)

	var btn_edit_rcss := Button.new()
	btn_edit_rcss.text = "Edit RCSS"
	btn_edit_rcss.tooltip_text = "Open associated RCSS files in the script editor"
	btn_edit_rcss.pressed.connect(_on_edit_rcss.bind(ctx))
	btn_row.add_child(btn_edit_rcss)

	var btn_reload := Button.new()
	btn_reload.text = "Reload"
	btn_reload.tooltip_text = "Reload all documents on this context"
	btn_reload.pressed.connect(_on_reload.bind(ctx))
	btn_row.add_child(btn_reload)

	container.add_child(btn_row)
	add_custom_control(container)

func _on_edit_rml(ctx: RmlContext) -> void:
	var path: String = ctx.get("document_path")
	if path.is_empty():
		return
	_open_in_editor(path)

func _on_edit_rcss(ctx: RmlContext) -> void:
	var rml_path: String = ctx.get("document_path")
	if rml_path.is_empty():
		return
	var rcss_paths := _extract_rcss_links(rml_path)
	for rcss_path in rcss_paths:
		_open_in_editor(rcss_path)

func _on_reload(ctx: RmlContext) -> void:
	if ctx.has_method("reload_all_documents"):
		ctx.reload_all_documents()

func _open_in_editor(path: String) -> void:
	if not FileAccess.file_exists(path):
		push_warning("RmlUI: File not found: %s" % path)
		return
	var res := load(path)
	if res:
		EditorInterface.edit_resource(res)

func _extract_rcss_links(rml_path: String) -> PackedStringArray:
	var result := PackedStringArray()
	if not FileAccess.file_exists(rml_path):
		return result
	var content := FileAccess.get_file_as_string(rml_path)
	var base_dir := rml_path.get_base_dir()
	# Find <link type="text/rcss" href="..."/>
	var regex := RegEx.new()
	regex.compile('href="([^"]+\\.rcss)"')
	for m in regex.search_all(content):
		var href: String = m.get_string(1)
		if href.begins_with("res://"):
			result.append(href)
		else:
			result.append(base_dir.path_join(href))
	return result
