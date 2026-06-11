extends SceneTree
## Error-recovery fuzz: feed hostile input through every text-accepting API.
## The bar: never crash, and the context must still load a good document
## afterwards (no poisoned state).

const GOOD_DOC := """<rml><head><style>body { font-family: "Noto Sans"; font-size: 14px; }</style></head>
<body><div id="ok">recovered</div></body></rml>"""

var _ctx: Node
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	create_timer(0.3).timeout.connect(_run)


func _run() -> void:
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")

	var nasty_docs := {
		"empty": "",
		"whitespace": "   \n\t  ",
		"not xml": "hello world & friends < > '",
		"truncated tag": "<rml><body><div",
		"unclosed everything": "<rml><head><style>body{<script>func x(:</head>",
		"mismatched tags": "<rml><body><div><span></div></span></body></rml>",
		"garbage bytes": "<rml>" + char(1) + char(7) + char(0xFFFD) + " xx</rml>",
		"huge attribute": "<rml><body><div class=\"%s\">x</div></body></rml>" % "a".repeat(100000),
		"deep nesting": "<rml><body>%s%s</body></rml>" % ["<div>".repeat(300), "</div>".repeat(300)],
		"bad entity": "<rml><body>&nope;&#xZZ;&</body></rml>",
		"script syntax error": "<rml><head><script>func broken(:\n\tpass</script></head><body onload=\"gdscript:_x\">x</body></rml>",
		"script runtime error": "<rml><head><script>func _on_load(_e):\n\tvar n = null\n\tn.boom()</script></head><body onload=\"gdscript:_on_load\">x</body></rml>",
		"bad data model": "<rml><body data-model=\"nope\"><div data-for=\"i : nope\">{{ i }}</div></body></rml>",
		"bad expression": "<rml><body data-model=\"m\"><div>{{ ((((( }}</div></body></rml>",
		"missing texture": "<rml><head><style>div { decorator: image(\"res://nope.png\"); }</style></head><body><div>x</div></body></rml>",
		"missing link": "<rml><head><link type=\"text/rcss\" href=\"res://does_not_exist.rcss\"/></head><body>x</body></rml>",
		"recursive template": "<rml><head><template name=\"t\" content=\"t\"/></head><body template=\"t\">x</body></rml>",
	}
	for name in nasty_docs:
		_ctx.call("load_document_from_string", nasty_docs[name], "memory://fuzz_doc")
		_ctx.call("unload_document", "memory://fuzz_doc")

	var nasty_rcss := [
		"", "}{}{}{", "body { color: ", "@media (min-width {{{", "/* unterminated",
		"body { %s: red; }" % "x".repeat(50000),
		"div::::pseudo { a:b }", "* { decorator: shader(nonexistent); }",
		"@spritesheet bad { src: res://nope.png; }",
	]
	for rcss in nasty_rcss:
		_ctx.call("inject_stylesheet", rcss)

	# Hostile API arguments on a context with no matching state.
	_ctx.call("reload_document", "never://loaded")
	_ctx.call("unload_document", "never://loaded")
	_ctx.call("load_document", "res://does_not_exist.rml")
	_ctx.call("load_font_face", "res://not_a_font.rml")
	_ctx.call("set_data_variable", "ghost_model", "x", 1)
	_ctx.call("set_data_array", "ghost_model", "arr", [1])
	_ctx.call("get_element_by_id", "")
	_ctx.call("set_element_inner_rml", "missing_element", "<b>x</b>")
	_ctx.call("get_document_script", "never://loaded")
	_ctx.call("set_dp_ratio", 0.0001)
	_ctx.call("set_dp_ratio", 1000.0)
	_ctx.call("set_dp_ratio", 1.0)

	# Survival check: context must still work normally.
	create_timer(0.3).timeout.connect(_verify)


func _verify() -> void:
	var ok = _ctx.call("load_document_from_string", GOOD_DOC, "memory://good")
	_check("good document loads after fuzzing", bool(ok))
	var info: Dictionary = _ctx.call("get_context_info")
	_check("context still initialized", info.get("initialized", false))
	var handle = _ctx.call("get_element_by_id", "ok")
	_check("element queries still work", handle != null)
	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
