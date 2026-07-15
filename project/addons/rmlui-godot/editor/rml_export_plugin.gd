@tool
extends EditorExportPlugin

## Auto-includes RML/RCSS documents in exported projects.
##
## Godot only packs recognised resources plus whatever matches an export
## preset's "non-resource file" filters. `.rml`/`.rcss` are plain text files
## with no importer, so they are silently dropped from every export unless the
## user remembers to add `*.rml, *.rcss` to those filters. RmlUi then can't open
## them at runtime (FileAccess fails on the missing path). This plugin scans the
## project at export time and packs every `.rml`/`.rcss` verbatim at its res://
## path, so FileAccess/RmlUi resolve them in exported builds exactly like in the
## editor.
##
## Toggle with the `rmlui/export/auto_include_documents` project setting.

const SETTING := "rmlui/export/auto_include_documents"
const EXTENSIONS := ["rml", "rcss"]

func _get_name() -> String:
	return "RmlUiDocumentExporter"

func _export_begin(features: PackedStringArray, is_debug: bool, path: String, flags: int) -> void:
	if not ProjectSettings.get_setting(SETTING, true):
		return
	var added := 0
	for file_path in _scan("res://"):
		var bytes := FileAccess.get_file_as_bytes(file_path)
		if bytes.is_empty() and FileAccess.get_open_error() != OK:
			push_warning("[RmlUi] Export: could not read %s" % file_path)
			continue
		# remap=false → the bytes are packed at file_path, so res://… still
		# resolves in the exported build.
		add_file(file_path, bytes, false)
		added += 1
	if added > 0:
		print("[RmlUi] Export: bundled %d RML/RCSS document(s)." % added)

## Recursively collect every res:// file whose extension is one of EXTENSIONS.
## The .godot cache and hidden folders are skipped.
func _scan(dir_path: String) -> PackedStringArray:
	var found := PackedStringArray()
	var dir := DirAccess.open(dir_path)
	if dir == null:
		return found
	dir.list_dir_begin()
	var name := dir.get_next()
	while name != "":
		if name.begins_with("."):
			name = dir.get_next()
			continue
		var full := dir_path.path_join(name)
		if dir.current_is_dir():
			found.append_array(_scan(full))
		elif name.get_extension().to_lower() in EXTENSIONS:
			found.append(full)
		name = dir.get_next()
	dir.list_dir_end()
	return found
