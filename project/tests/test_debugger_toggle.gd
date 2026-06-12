extends SceneTree
## debugger_toggle_key works WITHOUT focus (unhandled-input path) — the old
## _gui_input wiring required Control focus that nothing ever granted.

var _ctx: Node
var _docs_before := 0
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	create_timer(0.3).timeout.connect(_setup)


func _setup() -> void:
	_ctx.call("load_document_from_string", "<rml><head></head><body>x</body></rml>", "memory://dbg")
	_docs_before = int(_ctx.call("get_context_info").get("num_documents", 0))

	var ev := InputEventKey.new()
	ev.keycode = KEY_F10
	ev.pressed = true
	Input.parse_input_event(ev)
	create_timer(0.3).timeout.connect(_verify)


func _verify() -> void:
	var docs_after := int(_ctx.call("get_context_info").get("num_documents", 0))
	print("  documents before/after F10: %d -> %d" % [_docs_before, docs_after])
	_check("debugger documents appeared (no focus needed)", docs_after > _docs_before)
	_check("toggle_debugger() callable", _ctx.has_method("toggle_debugger"))
	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
