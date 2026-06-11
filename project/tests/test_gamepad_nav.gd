extends SceneTree
## Gamepad navigation: Godot's ui_* actions drive RmlUi's built-in focus
## engine — tab order, spatial arrow nav (nav-* properties), accept → click.
## Actions are synthesized with InputEventAction (what a gamepad binding
## resolves to), so this covers the whole bridge.

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 16px; width: 100%; height: 100%; }
button { display: block; width: 140px; height: 40px; margin: 8px; background-color: #334; tab-index: auto; nav: auto; }
button:focus { background-color: #66a; }
</style>
<script>
var rml_context

func _pick(event):
	rml_context.set_meta("picked", str(event.get("target_id", "")))
</script>
</head>
<body>
	<button id="btn1" onclick="gdscript:_pick">One</button>
	<button id="btn2" onclick="gdscript:_pick">Two</button>
	<button id="btn3" onclick="gdscript:_pick">Three</button>
</body>
</rml>"""

var _ctx: Node
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set("gamepad_navigation", true)
	create_timer(0.3).timeout.connect(_setup)


func _setup() -> void:
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	_ctx.call("load_document_from_string", DOC, "memory://nav")
	create_timer(0.3).timeout.connect(_run)


func _action(name: String) -> void:
	var ev := InputEventAction.new()
	ev.action = name
	ev.pressed = true
	Input.parse_input_event(ev)


func _focused(id: String) -> bool:
	var h = _ctx.call("get_element_by_id", id)
	return h != null and h.is_pseudo_class_set("focus")


func _run() -> void:
	# 1. First directional press with no focus grabs the first tabbable.
	_action("ui_down")
	await create_timer(0.15).timeout
	_check("first press focuses btn1", _focused("btn1"))

	# 2. Arrow down = spatial nav to the next button.
	_action("ui_down")
	await create_timer(0.15).timeout
	_check("ui_down moves to btn2", _focused("btn2"))

	_action("ui_down")
	await create_timer(0.15).timeout
	_check("ui_down moves to btn3", _focused("btn3"))

	# 3. Arrow up navigates back.
	_action("ui_up")
	await create_timer(0.15).timeout
	_check("ui_up returns to btn2", _focused("btn2"))

	# 4. Tab order forward.
	_action("ui_focus_next")
	await create_timer(0.15).timeout
	_check("ui_focus_next moves to btn3", _focused("btn3"))

	# 5. Accept clicks the focused element -> inline script handler.
	_action("ui_accept")
	await create_timer(0.15).timeout
	_check("ui_accept clicks focused btn3", str(_ctx.get_meta("picked", "")) == "btn3")

	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
