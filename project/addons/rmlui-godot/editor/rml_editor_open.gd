@tool
class_name RmlEditorOpen
extends RefCounted
## Single place that opens a project file in the Godot editor — used by the
## inspector's Edit RML/RCSS buttons and Ctrl-click link navigation (issue #23).
##
## .gd / scenes / resources have public open APIs (edit_script, edit_resource,
## open_scene_from_path). Plain text files (.rml/.rcss) do NOT: the editor
## exposes no "open text file" call (ScriptEditor has only close_file;
## FileSystemDock only navigate_to_path). For those we reproduce the FileSystem
## dock's double-click, which natively opens registered textfile extensions in
## the script editor — navigate to the file (which selects it in the dock's
## folder Tree and, in split layout, its file ItemList), then fire item_activated
## on whichever control holds the selection. The file-list selection can populate
## a frame late, so we wait one process frame before firing.


## Open `path` in the appropriate editor. Coroutine (awaits a frame for text
## files); callers may fire-and-forget.
static func open_file(path: String) -> void:
	if not FileAccess.file_exists(path):
		push_warning("RmlUI: file not found: %s" % path)
		return
	var ext := path.get_extension().to_lower()
	if ext == "tscn" or ext == "scn":
		EditorInterface.open_scene_from_path(path)
		return
	if ResourceLoader.exists(path):
		var res := load(path)
		if res is Script:
			EditorInterface.edit_script(res)
			EditorInterface.set_main_screen_editor("Script")
			return
		if res != null:
			EditorInterface.edit_resource(res)
			return
	await _open_text_file(path)


static func _open_text_file(path: String) -> void:
	var dock := EditorInterface.get_file_system_dock()
	if dock == null:
		EditorInterface.select_file(path)
		return
	dock.navigate_to_path(path)
	# The file-list selection can populate on the next frame after navigation.
	var loop := Engine.get_main_loop() as SceneTree
	if loop:
		await loop.process_frame
	if not _activate_selection(dock):
		# Couldn't simulate the open — at least leave the file selected so the
		# user is one click away from it.
		EditorInterface.select_file(path)


## Fire item_activated on every selected Tree/ItemList under the dock (the dock
## connects its open handler to these). Returns true if anything was fired.
static func _activate_selection(node: Node) -> bool:
	var fired := false
	for child in node.get_children():
		if child is Tree:
			if (child as Tree).get_selected() != null:
				(child as Tree).item_activated.emit()
				fired = true
		elif child is ItemList:
			var il := child as ItemList
			for idx in il.get_selected_items():
				il.item_activated.emit(idx)
				fired = true
		if _activate_selection(child):
			fired = true
	return fired
