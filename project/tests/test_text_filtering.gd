extends SceneTree
## text_filtering_mode applies an explicit filter to GLYPH draws only
## (default Nearest — crisp like Godot text — independent of the project
## default and the node texture_filter). Probe: at a fractional dp_ratio,
## glyphs sample off the texel grid, so Nearest vs Linear must differ.

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 13px; color: #ffffff; width: 100%; height: 100%; background-color: #181818; }
</style>
</head>
<body>
	<div>The quick brown fox jumps over the lazy dog 1234567890</div>
	<div>iiillllIIII mmmwww fiji ffl HP 1250/1250</div>
</body>
</rml>"""

var _sv: SubViewport
var _ctx: Node
var _img_default: Image
var _phase := 0
var _fails := 0


func _initialize() -> void:
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(400, 200)
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
	# Fractional scale → glyph quads off the texel grid → filter choice visible.
	_ctx.set("dp_ratio", 1.25)
	_ctx.call("load_document_from_string", DOC, "memory://filter")
	create_timer(0.6).timeout.connect(_step)


func _step() -> void:
	match _phase:
		0:
			_img_default = _sv.get_texture().get_image()
			_ctx.set("text_filtering_mode", 1)  # Linear (default is Nearest)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			var img_linear := _sv.get_texture().get_image()
			var diff := 0
			for y in range(0, img_linear.get_height(), 1):
				for x in range(0, img_linear.get_width(), 1):
					var a := _img_default.get_pixel(x, y)
					var b := img_linear.get_pixel(x, y)
					if absf(a.r - b.r) + absf(a.g - b.g) + absf(a.b - b.b) > 0.04:
						diff += 1
			print("  pixels differing nearest vs linear: ", diff)
			_check("text_filtering_mode reaches glyph draws", diff > 0)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
