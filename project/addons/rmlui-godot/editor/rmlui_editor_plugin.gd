@tool
extends EditorPlugin

var _rcss_highlighter: RcssSyntaxHighlighter
var _rml_highlighter: RmlSyntaxHighlighter
var _inspector_plugin: RmlInspectorPlugin
var _preview_panel: RmlPreviewPanel
var _completion_provider: RcssCompletionProvider
var _completion_timer: Timer

func _enter_tree():
	_rcss_highlighter = RcssSyntaxHighlighter.new()
	_rml_highlighter = RmlSyntaxHighlighter.new()
	var script_editor := EditorInterface.get_script_editor()
	script_editor.register_syntax_highlighter(_rcss_highlighter)
	script_editor.register_syntax_highlighter(_rml_highlighter)

	# Completion: the script editor has no hook for plain text files, so poll
	# for the current editor tab and attach to its CodeEdit when it's one of
	# ours (identified by syntax highlighter).
	_completion_provider = RcssCompletionProvider.new()
	_completion_timer = Timer.new()
	_completion_timer.wait_time = 0.5
	_completion_timer.timeout.connect(_ensure_completion_hook)
	add_child(_completion_timer)
	_completion_timer.start()

	_inspector_plugin = RmlInspectorPlugin.new()
	add_inspector_plugin(_inspector_plugin)

	_preview_panel = RmlPreviewPanel.new()
	add_control_to_bottom_panel(_preview_panel, "RmlUI Preview")

	var selection := EditorInterface.get_selection()
	selection.selection_changed.connect(_on_selection_changed)

func _exit_tree():
	var script_editor := EditorInterface.get_script_editor()
	if _rcss_highlighter:
		script_editor.unregister_syntax_highlighter(_rcss_highlighter)
		_rcss_highlighter = null
	if _rml_highlighter:
		script_editor.unregister_syntax_highlighter(_rml_highlighter)
		_rml_highlighter = null

	if _inspector_plugin:
		remove_inspector_plugin(_inspector_plugin)
		_inspector_plugin = null

	if _preview_panel:
		remove_control_from_bottom_panel(_preview_panel)
		_preview_panel.queue_free()
		_preview_panel = null

func _ensure_completion_hook() -> void:
	var ed := EditorInterface.get_script_editor().get_current_editor()
	if ed == null:
		return
	var ce := ed.get_base_editor() as CodeEdit
	if ce == null or ce.has_meta("rmlui_completion"):
		return
	var hl := ce.syntax_highlighter
	if hl is RcssSyntaxHighlighter or hl is RmlSyntaxHighlighter:
		ce.set_meta("rmlui_completion", true)
		ce.code_completion_enabled = true
		ce.code_completion_prefixes = [":"]
		ce.code_completion_requested.connect(_on_completion_requested.bind(ce))

func _on_completion_requested(ce: CodeEdit) -> void:
	if _completion_provider.fill_options(ce):
		ce.update_code_completion_options(true)

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
