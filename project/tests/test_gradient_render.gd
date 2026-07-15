extends SceneTree
## Regression test for issue #43: built-in gradient decorators (linear/radial/
## conic) must actually paint pixels. Before the fix, GodotRenderInterface::
## CompileShader only handled custom shader("<value>") decorators and returned 0
## for every gradient, so gradients rendered nothing (the page background showed
## through). This renders a document with the three gradient families over a pure
## green background and asserts each gradient region (a) is no longer green and
## (b) actually varies across the element (i.e. it's a gradient, not a flat fill).

const DOC := "res://tests/fixtures/gradients/gradients.rml"

var _sv: SubViewport
var _ctx: Node
var _fails := 0


func _initialize() -> void:
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(640, 360)
	root.add_child(svc)
	_sv = SubViewport.new()
	_sv.size = Vector2i(640, 360)
	_sv.transparent_bg = false
	_sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	svc.add_child(_sv)

	_ctx = ClassDB.instantiate(&"RmlContext")
	_sv.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	create_timer(0.3).timeout.connect(func():
		_ctx.call("load_document", DOC)
		create_timer(1.0).timeout.connect(_check))


func _is_green(c: Color) -> bool:
	# The page background is pure #00ff00. A painted gradient is never that.
	return c.g > 0.7 and c.r < 0.3 and c.b < 0.3


func _check() -> void:
	var img := _sv.get_texture().get_image()
	img.save_png("res://tests/_gradient_render.png")

	# --- radial: div at (10,10) 120x120, center (70,70), white -> black ---
	var r_center := img.get_pixel(70, 70)
	var r_edge := img.get_pixel(18, 18)
	_expect(not _is_green(r_center), "radial center painted (not background)", r_center)
	_expect(not _is_green(r_edge), "radial edge painted (not background)", r_edge)
	_expect(r_center.v - r_edge.v > 0.2, "radial varies center->edge (is a gradient)",
		Color(r_center.v, r_edge.v, 0))

	# --- linear: div at (150,10) 120x120, red(left) -> blue(right) ---
	var l_left := img.get_pixel(158, 70)
	var l_right := img.get_pixel(262, 70)
	_expect(not _is_green(l_left), "linear left painted (not background)", l_left)
	_expect(not _is_green(l_right), "linear right painted (not background)", l_right)
	_expect(l_left.r - l_right.r > 0.2, "linear red fades left->right", l_left)
	_expect(l_right.b - l_left.b > 0.2, "linear blue grows left->right", l_right)

	# --- conic: div at (290,10) 120x120 ---
	var c_a := img.get_pixel(350, 20)
	var c_b := img.get_pixel(350, 120)
	_expect(not _is_green(c_a), "conic top painted (not background)", c_a)
	_expect(not _is_green(c_b), "conic bottom painted (not background)", c_b)

	# --- repeating-linear: div at (430,10) 120x120, black/white bands every 20px ---
	# Exercises the repeating-* branch (func >= 3, driven by the "repeating" param).
	# A correct repeat shows several dark bands across the row; a non-repeating
	# fallback would ramp once and stay light. Count dark->light band starts.
	var rep_green := false
	var bands := 0
	var was_dark := false
	for x in range(431, 549):
		var px := img.get_pixel(x, 60)
		if _is_green(px):
			rep_green = true
		var dark := px.v < 0.25
		if dark and not was_dark:
			bands += 1
		was_dark = dark
	_expect(not rep_green, "repeating-linear painted (not background)", Color(0, float(bands), 0))
	_expect(bands >= 3, "repeating-linear actually repeats (%d dark bands)" % bands,
		Color(0, float(bands), 0))

	print("ALL PASSED" if _fails == 0 else ("%d CHECKS FAILED" % _fails))
	quit(_fails)


func _expect(cond: bool, label: String, sample: Color) -> void:
	if cond:
		print("  PASS: ", label)
	else:
		_fails += 1
		print("  FAIL: ", label, "  sample=", sample)
