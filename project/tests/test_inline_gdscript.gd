extends SceneTree
## End-to-end test for inline GDScript in RML documents:
## - <script> block compiled at load, instanced lazily on first dispatch
## - onload="gdscript:method" event attribute dispatch
## - `var rml_context` injection
## - fallback to the RmlContext's parent node method

const DOC_SCRIPT_BLOCK := """<rml>
<head>
<script>
var rml_context
var count := 0

func _on_load(event):
	count += 1
	if rml_context != null:
		rml_context.set_meta("script_block_ran", count)
		rml_context.set_meta("event_type", str(event.get("type", "")))
</script>
</head>
<body onload="gdscript:_on_load">
	<div>hello</div>
</body>
</rml>"""

const DOC_PARENT_FALLBACK := """<rml>
<head></head>
<body onload="gdscript:_parent_handler">
	<div>hello</div>
</body>
</rml>"""

const PARENT_SCRIPT := """extends Node

func _parent_handler(event):
	set_meta("parent_handler_ran", true)
"""

const DOC_SIGNAL := """<rml>
<head>
<script>
signal ping(message: String)

func send_ping():
	ping.emit("hello from the block")
</script>
</head>
<body><div>signal test</div></body>
</rml>"""

var _ping_received := ""

var _parent: Node
var _ctx: Node
var _phase := 0
var _fails := 0


func _initialize() -> void:
	_parent = Node.new()
	var script := GDScript.new()
	script.source_code = PARENT_SCRIPT
	script.reload()
	_parent.set_script(script)
	root.add_child(_parent)

	_ctx = ClassDB.instantiate(&"RmlContext")
	_parent.add_child(_ctx)
	create_timer(0.3).timeout.connect(_step)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document_from_string", DOC_SCRIPT_BLOCK, "memory://script_block")
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			_check("script block handler ran", int(_ctx.get_meta("script_block_ran", 0)) == 1)
			_check("rml_context injected", _ctx.has_meta("script_block_ran"))
			_check("event dict passed", str(_ctx.get_meta("event_type", "")) == "load")
			_ctx.call("unload_document", "memory://script_block")
			_ctx.call("load_document_from_string", DOC_PARENT_FALLBACK, "memory://fallback")
			_phase = 2
			create_timer(0.5).timeout.connect(_step)
		2:
			_check("parent node fallback ran", bool(_parent.get_meta("parent_handler_ran", false)))
			_ctx.call("unload_document", "memory://fallback")
			_ctx.call("load_document_from_string", DOC_SIGNAL, "memory://signal")
			_phase = 3
			create_timer(0.5).timeout.connect(_step)
		3:
			# Signal bridge: game code reaches the block instance via
			# get_document_script, connects to its signal, calls its method.
			var inst = _ctx.call("get_document_script", "memory://signal")
			_check("get_document_script returns instance", inst != null)
			if inst != null:
				inst.ping.connect(func(msg): _ping_received = msg)
				inst.send_ping()
			_check("signal received on godot side", _ping_received == "hello from the block")
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
