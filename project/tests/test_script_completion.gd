extends SceneTree
## Reflection-based GDScript completion inside <script> blocks: data layer.

const Provider := preload("res://addons/rmlui-godot/editor/rcss_completion_provider.gd")

var _fails := 0


func _initialize() -> void:
	var c = ClassDB.instantiate(&"RmlContext")  # ensures RmlContext is registered
	root.add_child(c)
	create_timer(0.2).timeout.connect(_run)


func _run() -> void:
	var p := Provider.new()

	# rml_context. → real RmlContext members via reflection.
	var opts: Array = Provider.class_member_options("RmlContext")
	var names := opts.map(func(o): return String(o[1]))
	_check("has load_document(...)", names.any(func(n): return n.begins_with("load_document(")))
	_check("has set_data_variable(...)", names.any(func(n): return n.begins_with("set_data_variable(")))
	_check("has rml_input_action signal", "rml_input_action" in names)
	_check("has document_path property", "document_path" in names)
	_check("no group markers", not names.any(func(n): return n.contains("/")))

	# zero-arg insert closes parens; with-args leaves open.
	var ins := {}
	for o in opts:
		ins[o[1]] = o[2]
	_check("zero-arg closes parens", String(ins.get("reload_all_documents()", "")) == "reload_all_documents()")
	_check("with-args leaves open", String(ins.get("load_document(path)", "")) == "load_document(")

	# Engine singleton classes resolve too.
	var input_names: Array = Provider.class_member_options("Input").map(func(o): return String(o[1]))
	_check("Input.is_action_pressed via reflection", input_names.any(func(n): return n.begins_with("is_action_pressed(")))

	# Buffer-local declarations.
	var locals: Array = p._buffer_locals("var rml_context\nvar count := 0\nsignal done\nfunc _add(e):\n\tpass\n")
	var lnames := locals.map(func(l): return String(l[1]))
	_check("locals found", "count" in lnames and "done" in lnames and "_add" in lnames)

	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
