@tool
extends EditorPlugin

var _rcss_highlighter: RcssSyntaxHighlighter
var _inspector_plugin: RmlInspectorPlugin

func _enter_tree():
	_rcss_highlighter = RcssSyntaxHighlighter.new()
	var script_editor := EditorInterface.get_script_editor()
	script_editor.register_syntax_highlighter(_rcss_highlighter)

	_inspector_plugin = RmlInspectorPlugin.new()
	add_inspector_plugin(_inspector_plugin)

func _exit_tree():
	if _rcss_highlighter:
		var script_editor := EditorInterface.get_script_editor()
		script_editor.unregister_syntax_highlighter(_rcss_highlighter)
		_rcss_highlighter = null

	if _inspector_plugin:
		remove_inspector_plugin(_inspector_plugin)
		_inspector_plugin = null
