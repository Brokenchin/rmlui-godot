extends SceneTree
## <script> blocks compile with newline padding so GDScript-reported line
## numbers (parse errors, runtime stack frames) equal the .rml file line.

const DOC := """<rml>
<head>
<script>
var rml_context
var marker := 1

func _on_load(_e):
	pass
</script>
</head>
<body onload="gdscript:_on_load">x</body>
</rml>"""

var _ctx: Node
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	create_timer(0.3).timeout.connect(_run)


func _run() -> void:
	_ctx.call("load_document_from_string", DOC, "memory://linemap")
	var inst = _ctx.call("get_document_script", "memory://linemap")
	_check("script instance exists", inst != null)
	if inst != null:
		var src: String = inst.get_script().source_code
		var src_lines := src.split("\n")
		var doc_lines := DOC.split("\n")
		var src_idx := -1
		var doc_idx := -1
		for i in src_lines.size():
			if src_lines[i].begins_with("var marker"):
				src_idx = i
		for i in doc_lines.size():
			if doc_lines[i].begins_with("var marker"):
				doc_idx = i
		print("  marker at compiled line %d, rml line %d" % [src_idx + 1, doc_idx + 1])
		_check("compiled line == rml line", src_idx == doc_idx and src_idx != -1)
	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
