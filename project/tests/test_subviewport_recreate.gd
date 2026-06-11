extends SceneTree
## Repro for "preview panel only renders the first context".
## Mirrors RmlPreviewPanel: SubViewport + RmlContext, then free + recreate.
## Run windowed: godot --path . -s tests/test_subviewport_recreate.gd

var _svc: SubViewportContainer
var _sv: SubViewport
var _ctx: Node
var _round := 0
var _fails := 0


func _initialize() -> void:
	_svc = SubViewportContainer.new()
	_svc.stretch = true
	_svc.position = Vector2(20, 20)
	_svc.size = Vector2(640, 360)
	root.add_child(_svc)

	_sv = SubViewport.new()
	_sv.transparent_bg = true
	_sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_svc.add_child(_sv)

	_spawn_context()
	create_timer(1.0).timeout.connect(_check_round)


func _spawn_context() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	_ctx.name = "Preview%d" % _round
	_sv.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	# Defer loads one tick so RmlContext._ready has initialized RmlUi.
	create_timer(0.1).timeout.connect(_configure_context)


func _configure_context() -> void:
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")
	_ctx.call("load_document", "res://addons/rmlui-godot/examples/basic/hello_world/hello.rml")


func _check_round() -> void:
	var img := _sv.get_texture().get_image()
	var colored := 0
	for y in range(0, img.get_height(), 4):
		for x in range(0, img.get_width(), 4):
			if img.get_pixel(x, y).a > 0.05:
				colored += 1
	var ok := colored > 50
	var info: Dictionary = _ctx.call("get_context_info")
	print("ROUND %d: %d colored px, ctx_size=%s, info=%s -> %s" % [
		_round, colored, _ctx.get_size(), info, "PASS" if ok else "FAIL"])
	if not ok:
		_fails += 1

	_round += 1
	match _round:
		1:
			# Round 1: recreate WITHOUT freeing the old one (old stays alive).
			_ctx.queue_free()  # actually free it — mirror panel behavior
			_spawn_context()
			create_timer(1.0).timeout.connect(_check_round)
		2:
			# Round 2: spawn ANOTHER context alongside (no free this time).
			_spawn_context()
			create_timer(1.0).timeout.connect(_check_round)
		_:
			print("ALL PASSED" if _fails == 0 else "%d ROUND(S) FAILED" % _fails)
			quit(_fails)
