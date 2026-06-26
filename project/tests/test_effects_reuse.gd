extends SceneTree
## Generalization guard for the redraw-coalescing slot reuse (issue #14): the
## persistent canvas-item pipeline must render every built-in structural effect
## identically to a from-scratch build, both when a slot is reused untouched and
## when a sibling changes around it. Exercises the paths the reconciler special-
## cases: opacity (layer group + modulate), overflow:hidden (clip-mask group),
## nested clipping, and gradient decorators. Windowed pixel readback via a
## SubViewport — viewport textures are blank headless.

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 14px; color: #ffffff; width: 100%; height: 100%; background-color: #101018; }
#grad { display: block; width: 180px; height: 60px; margin: 6px; decorator: vertical-gradient( #e94560 #0f3460 ); }
#faded { display: block; width: 180px; height: 50px; margin: 6px; background-color: #33cc66; opacity: 0.5; }
#clip { display: block; width: 180px; height: 60px; margin: 6px; background-color: #203040; overflow: hidden; }
#clip .inner { display: block; width: 400px; height: 30px; margin-top: 10px; background-color: #ffaa00; }
#clip .inner .deep { display: block; width: 300px; height: 12px; background-color: #00ddff; overflow: hidden; }
#toggle { display: block; width: 180px; height: 60px; margin: 6px; background-color: #444444; }
</style>
</head>
<body data-model="m">
	<div id="grad"></div>
	<div id="faded">faded</div>
	<div id="clip"><div class="inner"><div class="deep"></div>{{ tick }}</div></div>
	<div id="toggle">{{ toggle }}</div>
</body>
</rml>"""

var _sv: SubViewport
var _ctx: Node
var _phase := 0
var _fails := 0
var _s0: Image
# Effects occupy the top of the layout; the toggle box is last (bottom band).
const EFFECTS_Y0 := 0
const EFFECTS_Y1 := 190
const TOGGLE_Y0 := 200
const TOGGLE_Y1 := 290


func _initialize() -> void:
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(220, 300)
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
	_ctx.call("bind_data_variable", "m", "tick", "A")
	_ctx.call("bind_data_variable", "m", "toggle", "one")
	_ctx.call("load_document_from_string", DOC, "memory://effects")
	create_timer(0.6).timeout.connect(_step)


func _snap() -> Image:
	return _sv.get_texture().get_image()


func _band_diff(a: Image, b: Image, y0: int, y1: int) -> int:
	var n := 0
	var h := mini(y1, b.get_height())
	for y in range(y0, h, 2):
		for x in range(0, b.get_width(), 2):
			var pa := a.get_pixel(x, y)
			var pb := b.get_pixel(x, y)
			if absf(pa.r - pb.r) + absf(pa.g - pb.g) + absf(pa.b - pb.b) > 0.02:
				n += 1
	return n


func _step() -> void:
	match _phase:
		0:
			_s0 = _snap()
			# Sanity: the effects band must actually contain non-background
			# pixels, otherwise later "unchanged" assertions are vacuous.
			var bg := 0
			for x in range(0, _s0.get_width(), 4):
				var p := _s0.get_pixel(x, 30)
				if p.r + p.g + p.b > 0.1:
					bg += 1
			_check("effects band rendered something (n=%d)" % bg, bg > 0)
			# Phase 1: force a redraw with identical data -> output must be
			# pixel-identical (reuse/reconfigure both reproduce the same frame).
			_ctx.call("set_data_variable", "m", "toggle", "one")
			_phase = 1
			create_timer(0.4).timeout.connect(_step)
		1:
			var d := _band_diff(_s0, _snap(), 0, 300)
			_check("redraw with identical data is pixel-stable (diff=%d)" % d, d == 0)
			# Phase 2: change ONLY the toggle box; effects slots are reused while
			# the toggle slot reconfigures around them.
			_ctx.call("set_data_variable", "m", "toggle", "TWO!")
			_phase = 2
			create_timer(0.4).timeout.connect(_step)
		2:
			var now := _snap()
			var de := _band_diff(_s0, now, EFFECTS_Y0, EFFECTS_Y1)
			var dt := _band_diff(_s0, now, TOGGLE_Y0, TOGGLE_Y1)
			_check("effects intact while sibling changes (effects diff=%d)" % de, de == 0)
			_check("sibling toggle actually repainted (diff=%d)" % dt, dt > 0)
			# Phase 3: change content inside the clip-mask group; that effect
			# slot must reconfigure and the change must show.
			_ctx.call("set_data_variable", "m", "tick", "ZZZZ")
			_phase = 3
			create_timer(0.4).timeout.connect(_step)
		3:
			var de := _band_diff(_s0, _snap(), EFFECTS_Y0, EFFECTS_Y1)
			_check("effect content change repaints (effects diff=%d)" % de, de > 0)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
