extends SceneTree
## Regression for the visual-parity desync: state mutations from game code
## (no mouse/no input) must repaint. The dirty-flag render gate previously
## skipped DOM-API, data-binding and element-handle mutations entirely —
## changes stayed invisible until input forced a redraw.

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 20px; width: 100%; height: 100%; background-color: #181818; }
#label { display: block; color: #ffffff; }
#box { display: block; width: 120px; height: 60px; background-color: #303030; }
</style>
</head>
<body data-model="m">
	<div id="label">initial</div>
	<div id="box">{{ value }}</div>
</body>
</rml>"""

var _sv: SubViewport
var _ctx: Node
var _before: Image
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
	create_timer(0.3).timeout.connect(_setup)


func _setup() -> void:
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	_ctx.call("create_data_model", "m")
	_ctx.call("bind_data_variable", "m", "value", "AAA")
	_ctx.call("load_document_from_string", DOC, "memory://dirty")
	create_timer(0.6).timeout.connect(_step)


func _snap() -> Image:
	return _sv.get_texture().get_image()


func _diff(a: Image, b: Image) -> int:
	var n := 0
	for y in range(0, b.get_height(), 2):
		for x in range(0, b.get_width(), 2):
			var pa := a.get_pixel(x, y)
			var pb := b.get_pixel(x, y)
			if absf(pa.r - pb.r) + absf(pa.g - pb.g) + absf(pa.b - pb.b) > 0.1:
				n += 1
	return n


func _step() -> void:
	match _phase:
		0:
			_before = _snap()
			_ctx.call("set_element_inner_rml", "label", "CHANGED VIA DOM API")
			_phase = 1
			create_timer(0.4).timeout.connect(_step)
		1:
			var d := _diff(_before, _snap())
			_check("DOM API repaints without input (diff=%d)" % d, d > 0)
			_before = _snap()
			_ctx.call("set_data_variable", "m", "value", "BBBBBBBB")
			_phase = 2
			create_timer(0.4).timeout.connect(_step)
		2:
			var d := _diff(_before, _snap())
			_check("data binding repaints without input (diff=%d)" % d, d > 0)
			_before = _snap()
			var handle = _ctx.call("get_element_by_id", "box")
			if handle != null:
				handle.set_property("background-color", "#cc3344")
			_phase = 3
			create_timer(0.4).timeout.connect(_step)
		3:
			var d := _diff(_before, _snap())
			_check("element handle repaints without input (diff=%d)" % d, d > 0)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
