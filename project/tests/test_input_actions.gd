extends SceneTree
## Input-action forwarding: watched InputMap actions reach <script> blocks
## (_on_input_action) and the rml_input_action signal.

const DOC := """<rml>
<head>
<script>
var rml_context

func _on_input_action(action: String, pressed: bool):
	if pressed:
		rml_context.set_meta("last_action", action)
	else:
		rml_context.set_meta("last_release", action)
</script>
</head>
<body><div>input test</div></body>
</rml>"""

var _ctx: Node
var _signal_log := []
var _fails := 0


func _initialize() -> void:
	# Runtime InputMap action bound to a key.
	InputMap.add_action("rml_test_action")
	var key := InputEventKey.new()
	key.physical_keycode = KEY_F7
	InputMap.action_add_event("rml_test_action", key)

	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set("input_actions", PackedStringArray(["rml_test_action"]))
	create_timer(0.3).timeout.connect(_run)


func _run() -> void:
	_ctx.call("load_document_from_string", DOC, "memory://input")
	_ctx.connect("rml_input_action", func(action, pressed): _signal_log.append([action, pressed]))

	var press := InputEventKey.new()
	press.physical_keycode = KEY_F7
	press.pressed = true
	Input.parse_input_event(press)
	var release := InputEventKey.new()
	release.physical_keycode = KEY_F7
	release.pressed = false
	Input.parse_input_event(release)

	create_timer(0.3).timeout.connect(_verify)


func _verify() -> void:
	_check("script block got press", str(_ctx.get_meta("last_action", "")) == "rml_test_action")
	_check("script block got release", str(_ctx.get_meta("last_release", "")) == "rml_test_action")
	_check("signal fired twice", _signal_log.size() == 2)
	if _signal_log.size() == 2:
		_check("signal press order", _signal_log[0][1] == true and _signal_log[1][1] == false)
	# Disconnect lambda from the context (Node, dies with tree — safe), but
	# disconnect anyway for hygiene before quit.
	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
