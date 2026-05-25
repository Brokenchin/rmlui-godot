@tool
extends EditorPlugin

var _rcss_highlighter: RcssSyntaxHighlighter
var _inspector_plugin: RmlInspectorPlugin
var _preview_panel: RmlPreviewPanel

func _enter_tree():
	_rcss_highlighter = RcssSyntaxHighlighter.new()
	var script_editor := EditorInterface.get_script_editor()
	script_editor.register_syntax_highlighter(_rcss_highlighter)

	_inspector_plugin = RmlInspectorPlugin.new()
	add_inspector_plugin(_inspector_plugin)

	_preview_panel = RmlPreviewPanel.new()
	add_control_to_bottom_panel(_preview_panel, "RmlUI Preview")

	var selection := EditorInterface.get_selection()
	selection.selection_changed.connect(_on_selection_changed)

func _exit_tree():
	if _rcss_highlighter:
		var script_editor := EditorInterface.get_script_editor()
		script_editor.unregister_syntax_highlighter(_rcss_highlighter)
		_rcss_highlighter = null

	if _inspector_plugin:
		remove_inspector_plugin(_inspector_plugin)
		_inspector_plugin = null

	if _preview_panel:
		remove_control_from_bottom_panel(_preview_panel)
		_preview_panel.queue_free()
		_preview_panel = null

func _on_selection_changed() -> void:
	if not _preview_panel:
		return
	var selected := EditorInterface.get_selection().get_selected_nodes()
	var ctx: Node = null
	for node in selected:
		if node.is_class("RmlContext"):
			ctx = node
			break
	_preview_panel.track_context(ctx)
	if ctx:
		make_bottom_panel_item_visible(_preview_panel)
