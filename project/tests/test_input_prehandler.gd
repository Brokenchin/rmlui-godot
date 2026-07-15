extends SceneTree
## Issue #41: game-first input pre-handler + per-frame tick.
##
## Asserts the registered pre-handler runs on _gui_input BEFORE RmlUi, that a
## true return consumes the event (RmlUi never sees the press, so its mousedown
## listener doesn't fire) while a false return forwards it (the listener fires),
## that clearing the handler stops delivery, and that the tick is invoked every
## frame with a positive delta.
##
## Windowed (not headless): input is injected through the root viewport so it
## routes into the SubViewportContainer with the mouse-over state Godot needs,
## and the context must tick frames for _process (the tick) to run. Mirrors the
## injection approach in test_hover_bridge.gd.

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 16px; width: 100%; height: 100%; background-color: #202020; }
#btn { display: block; width: 120px; height: 50px; background-color: #3a4254; color: #ffffff; }
</style>
</head>
<body><div id="btn">click</div></body>
</rml>"""

var _sv: SubViewport
var _ctx: Node
var _phase := 0
var _fails := 0

var _pre_buttons := []        # InputEventMouseButton instances seen by the pre-handler
var _consume_next := false    # value the pre-handler returns for button events
var _rml_mousedown := 0       # times RmlUi delivered mousedown to #btn
var _tick_count := 0
var _tick_delta_sum := 0.0
var _tick_baseline := 0


func _initialize() -> void:
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(400, 300)
	root.add_child(svc)
	_sv = SubViewport.new()
	_sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	svc.add_child(_sv)

	_ctx = ClassDB.instantiate(&"RmlContext")
	_sv.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	create_timer(0.3).timeout.connect(_step)


# Records button events only; the motion used to position the cursor is noise.
func _prehandler(event: InputEvent) -> bool:
	if event is InputEventMouseButton:
		_pre_buttons.append(event)
		return _consume_next
	return false


func _tick(delta: float) -> void:
	_tick_count += 1
	_tick_delta_sum += delta


func _on_rml_mousedown(_dict: Dictionary) -> void:
	_rml_mousedown += 1


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
			_ctx.call("load_document_from_string", DOC, "memory://prehandler")
			_ctx.call("set_input_prehandler", _prehandler)
			_ctx.call("set_input_tick", _tick)
			_ctx.call("add_event_listener", "btn", "mousedown", _on_rml_mousedown)
			_advance(0.6)
		1:
			# Tick has been running every frame since registration.
			_check("tick fired across frames", _tick_count > 0)
			_check("tick delta is positive", _tick_delta_sum > 0.0)
			_check("pre-handler is reported via getter",
				_ctx.call("get_input_prehandler") == Callable(self, "_prehandler"))

			# Consume the press: RmlUi must not receive it.
			_consume_next = true
			_press(Vector2(60, 25))
			_advance(0.3)
		2:
			_check("pre-handler saw the consumed press", _pre_buttons.size() == 1)
			_check("consumed press did NOT reach RmlUi", _rml_mousedown == 0)

			# Forward the next press: RmlUi must receive it.
			_consume_next = false
			_press(Vector2(60, 25))
			_advance(0.3)
		3:
			_check("pre-handler saw the forwarded press", _pre_buttons.size() == 2)
			_check("forwarded press reached RmlUi (mousedown fired)", _rml_mousedown == 1)

			# Clear the handler: no further delivery.
			_ctx.call("set_input_prehandler", Callable())
			_check("cleared pre-handler reads back empty",
				not (_ctx.call("get_input_prehandler") as Callable).is_valid())
			_press(Vector2(60, 25))
			_advance(0.3)
		4:
			_check("cleared pre-handler stops receiving events", _pre_buttons.size() == 2)
			_check("press after clear still reaches RmlUi", _rml_mousedown == 2)

			# Clearing the tick stops it.
			var before := _tick_count
			_ctx.call("set_input_tick", Callable())
			_tick_baseline = before
			_advance(0.3)
		5:
			_check("cleared tick stops firing", _tick_count == _tick_baseline)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _advance(delay: float) -> void:
	_phase += 1
	create_timer(delay).timeout.connect(_step)


# Position the cursor with a motion event (so the container forwards subsequent
# input with mouse-over state), then push a left-button press at the same point.
func _press(pos: Vector2) -> void:
	var motion := InputEventMouseMotion.new()
	motion.position = pos
	motion.global_position = pos
	root.push_input(motion, true)

	var btn := InputEventMouseButton.new()
	btn.button_index = MOUSE_BUTTON_LEFT
	btn.pressed = true
	btn.position = pos
	btn.global_position = pos
	root.push_input(btn, true)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
