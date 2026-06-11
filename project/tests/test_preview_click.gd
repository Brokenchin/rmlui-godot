extends SceneTree
## Reproduces the editor preview's input path: RmlContext inside a
## SubViewport, mouse events delivered via Viewport.push_input (the panel's
## forwarding). Verifies inline gdscript click handlers run AND the render
## updates afterwards.

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 18px; width: 100%; height: 100%; background-color: #202020; }
button { display: block; width: 200px; height: 60px; background-color: #3a4254; color: #ffffff; }
#count { display: block; color: #99ccff; font-size: 24px; }
</style>
<script>
var rml_context
var count := 0

func _add(_event):
	count += 1
	rml_context.set_meta("clicked_count", count)
	var el = rml_context.get_element_by_id("count")
	if el:
		el.set_inner_rml(str(count))
</script>
</head>
<body>
	<button onclick="gdscript:_add">Add</button>
	<span id="count">0</span>
</body>
</rml>"""

var _sv: SubViewport
var _ctx: Node
var _img_before: Image
var _phase := 0
var _fails := 0


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


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
			_ctx.call("load_document_from_string", DOC, "memory://preview_click")
			_phase = 1
			create_timer(0.7).timeout.connect(_step)
		1:
			_img_before = _sv.get_texture().get_image()
			# Click the button center (~100, 30) the way the preview panel
			# forwards events: motion first (RmlUi uses the last move pos),
			# then press+release.
			_push_motion(Vector2(100, 30))
			_push_click(Vector2(100, 30), true)
			_push_click(Vector2(100, 30), false)
			_phase = 2
			create_timer(0.7).timeout.connect(_step)
		2:
			var clicked: int = int(_ctx.get_meta("clicked_count", 0))
			_check("handler dispatched via push_input", clicked == 1)

			var img_after := _sv.get_texture().get_image()
			var diff := 0
			for y in range(0, img_after.get_height(), 2):
				for x in range(0, img_after.get_width(), 2):
					var a := _img_before.get_pixel(x, y)
					var b := img_after.get_pixel(x, y)
					if absf(a.r - b.r) + absf(a.g - b.g) + absf(a.b - b.b) > 0.1:
						diff += 1
			print("  pixels changed after click: ", diff)
			_check("render updated after click", diff > 0)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _push_motion(pos: Vector2) -> void:
	var ev := InputEventMouseMotion.new()
	ev.position = pos
	ev.global_position = pos
	_sv.push_input(ev, true)


func _push_click(pos: Vector2, pressed: bool) -> void:
	var ev := InputEventMouseButton.new()
	ev.position = pos
	ev.global_position = pos
	ev.button_index = MOUSE_BUTTON_LEFT
	ev.pressed = pressed
	_sv.push_input(ev, true)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
