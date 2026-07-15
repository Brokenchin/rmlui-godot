extends SceneTree
## Issue #61 (render half) — pixel regression. A rounded element (border-radius +
## overflow:hidden) clips its content with a CLIP MASK, not a scissor. Inside an
## embedded scroll region, a slot scrolled past the viewport had its content
## scissor-culled but its rounded mask quad (drawn with the white texture) emitted
## unconditionally — leaking as a WHITE SQUARE outside the embed. The fix gives the
## clip-mask draw the same scissor cull/clip as ordinary geometry. This is a render
## bug (the leaked draw isn't hit-testable), so it's verified by reading pixels:
## after the fix there are no white mask pixels in the band just below the bag.
##
## Windowed (needs real pixels) — see tests/run_all.sh.

const PANEL := "res://tests/fixtures/realbag/realbag_panel.rml"
const N := 120

var _ctx: Node
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_ctx.set_size(Vector2(720, 760))
	# The context initializes on _ready (first frame) — defer the load like the
	# other embed tests do.
	create_timer(0.3).timeout.connect(_start)


func _start() -> void:
	var img := Image.create(24, 24, false, Image.FORMAT_RGBA8)
	img.fill(Color(0.9, 0.7, 0.15, 1.0))
	for k in 24:
		img.set_pixel(k, k, Color(0.2, 0.8, 1.0))
	_ctx.call("register_texture", "texture://icon", ImageTexture.create_from_image(img))
	# The clip-mask cull is universal (not embed-specific); the rounded-tablet bag in
	# a context larger than itself reproduces the white-mask leak deterministically.
	_ctx.call("load_document", PANEL)
	create_timer(0.6).timeout.connect(_fill)


func _slots() -> String:
	var s := "<div class=\"slot-grid\">"
	for i in N:
		s += "<div id=\"slot_%d\" class=\"slot\"><div class=\"slot-box\"><img class=\"slot-icon\" src=\"texture://icon\"/></div></div>" % i
	s += "</div>"
	return s


func _fill() -> void:
	_ctx.call("set_element_inner_rml", "grid", _slots())
	create_timer(0.4).timeout.connect(_scroll)


func _scroll() -> void:
	# Scroll the bag so rows leave the viewport (the partial/just-past rows are what
	# leak their white mask quad).
	for n in 24:
		var p := Vector2(120, 250)
		var e := InputEventMouseButton.new()
		e.button_index = MOUSE_BUTTON_WHEEL_DOWN
		e.position = p
		e.global_position = p
		e.pressed = true
		e.factor = 1.0
		_ctx.get_viewport().push_input(e, true)
	create_timer(0.4).timeout.connect(_grab)


func _grab() -> void:
	RenderingServer.frame_post_draw.connect(_check, Object.CONNECT_ONE_SHOT)


func _check() -> void:
	var image := root.get_viewport().get_texture().get_image()
	var bag = _ctx.call("get_element_by_id", "bag-root")
	var ok_bag: bool = bag != null and bag.is_valid()
	_assert("bag laid out", ok_bag)
	if not ok_bag:
		print("%d FAILED" % _fails)
		quit(_fails)
		return

	var pos: Vector2 = bag.get_position()
	var size: Vector2 = bag.get_size()
	# A band just below the bag's bottom edge, across its width. With the leak this
	# band is full of white mask squares; clipped, it's the host backdrop.
	var y0 := int(pos.y + size.y) + 2
	var y1 := y0 + 70
	var x0 := int(pos.x) + 2
	var x1 := int(pos.x + size.x) - 2
	var w := image.get_size()
	y1 = mini(y1, int(w.y))
	x1 = mini(x1, int(w.x))

	var white := 0
	var total := 0
	for y in range(y0, y1):
		for x in range(x0, x1):
			total += 1
			var c := image.get_pixel(x, y)
			if c.r > 0.8 and c.g > 0.8 and c.b > 0.8:
				white += 1
	print("  [info] bag=%s scan band y=%d..%d -> %d/%d white px" % [Rect2(pos, size), y0, y1, white, total])
	_assert("no clip-mask (white) leak in the band below the bag", white == 0)

	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _assert(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
