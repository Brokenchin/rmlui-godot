extends Control

const FONT_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const DOC_PATH := "res://addons/rmlui-godot/examples/stress/zindex_stacking/zindex_stacking.rml"
const ATLAS_PATH := "res://addons/rmlui-godot/examples/assets/fruits_atlas.png"

@onready var rml: RmlContext = $RmlContext


func _ready() -> void:
	rml.load_font_face(FONT_PATH)
	rml.load_document(DOC_PATH)

	await get_tree().process_frame

	_setup_test_textures()
	_setup_decorators()


func _setup_test_textures() -> void:
	var red_tex := _make_color_texture(Color(0.9, 0.2, 0.2), 80, 80)
	var green_tex := _make_color_texture(Color(0.2, 0.9, 0.3), 80, 80)
	var blue_tex := _make_color_texture(Color(0.2, 0.3, 0.9), 60, 60)
	var slot_tex := _make_color_texture(Color(0.8, 0.6, 0.1), 48, 48)

	rml.register_texture("tex://red", red_tex)
	rml.register_texture("tex://green", green_tex)
	rml.register_texture("tex://blue", blue_tex)
	rml.register_texture("tex://slot", slot_tex)

	# T2: img element tests
	_set_img_src("t2-img", "tex://red")
	_set_img_src("t2-img-rel", "tex://red")

	# T6: img vs div z-index
	_set_img_src("t6-img", "tex://green")

	# T9: img relative vs text absolute
	_set_img_src("t9-img", "tex://blue")

	# T10: multiple imgs
	_set_img_src("t10-img1", "tex://red")
	_set_img_src("t10-img2", "tex://green")

	# T13: overflow:hidden slots with img
	_set_img_src("t13-img1", "tex://slot")
	_set_img_src("t13-img2", "tex://slot")
	_set_img_src("t13-img3", "tex://slot")

	# T15: inline-block slots without overflow
	_set_img_src("t15-img1", "tex://slot")
	_set_img_src("t15-img2", "tex://slot")

	# T16: static img in dynamic test
	_set_img_src("t16-s-img", "tex://slot")


func _setup_decorators() -> void:
	# T3: decorator image on a div
	rml.set_element_property("t3-deco", "decorator", "image(tex://red)")

	# T11: decorator divs for stacking test
	rml.set_element_property("t11-back", "decorator", "image(tex://red)")
	rml.set_element_property("t11-front", "decorator", "image(tex://green)")

	# T12: text inside decorator div
	rml.set_element_property("t12-cell", "decorator", "image(tex://blue)")

	# T14: overflow:hidden + decorator
	rml.set_element_property("t14-cell1", "decorator", "image(tex://slot)")
	rml.set_element_property("t14-cell2", "decorator", "image(tex://slot)")

	_setup_dynamic_tests()


func _setup_dynamic_tests() -> void:
	# T16: dynamic inner RML — img variant
	var img_html := '<img class="t13-img" src="tex://slot" /><div class="t13-text">Dynamic</div>'
	rml.set_element_inner_rml("t16-dynamic-img", img_html)

	# T16: dynamic inner RML — decorator variant
	var deco_html := '<div class="t14-cell" style="decorator: image(tex://slot);"><div class="t14-text">DynDeco</div></div>'
	rml.set_element_inner_rml("t16-dynamic-deco", deco_html)

	_setup_parity_repro_tests()


func _setup_parity_repro_tests() -> void:
	# T17a: FLAT — img + text directly in slot (like T13 but dynamically injected)
	var t17a := '<img class="t17-icon" src="tex://slot" /><div class="t17-name">Flat</div>'
	rml.set_element_inner_rml("t17a", t17a)

	# T17b: NESTED — slot-cell wrapper → img + text (EXACT parity failing structure)
	var t17b := '<div class="t17-cell"><img class="t17-icon" src="tex://slot" /><div class="t17-name">Nested</div></div>'
	rml.set_element_inner_rml("t17b", t17b)

	# T17c: NESTED — slot-cell wrapper + decorator (EXACT parity working structure)
	var t17c := '<div class="t17-cell" style="decorator: image(tex://slot);"><div class="t17-name">Deco</div></div>'
	rml.set_element_inner_rml("t17c", t17c)

	# T17d: FLAT — decorator on the slot itself + text child
	rml.set_element_property("t17d", "decorator", "image(tex://slot)")
	var t17d := '<div class="t17-name">FlatD</div>'
	rml.set_element_inner_rml("t17d", t17d)

	# T18: Full grid injection — exact parity inventory flow
	# Reproduces _populate_rml_from_store() building entire grid HTML at once
	var img_grid := ""
	var deco_grid := ""
	for i in range(4):
		var label := "I%d" % i
		img_grid += '<div class="t17-slot"><div class="t17-cell"><img class="t17-icon" src="tex://slot" /><div class="t17-name">%s</div></div></div>' % label
		deco_grid += '<div class="t17-slot"><div class="t17-cell" style="decorator: image(tex://slot);"><div class="t17-name">%s</div></div></div>' % label

	rml.set_element_inner_rml("t18-img-grid", img_grid)
	rml.set_element_inner_rml("t18-deco-grid", deco_grid)


func _set_img_src(element_id: String, tex_url: String) -> void:
	rml.set_element_attribute(element_id, "src", tex_url)


func _make_color_texture(color: Color, w: int, h: int) -> ImageTexture:
	var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
	img.fill(color)
	# Draw a 2px border for visual clarity
	var border_color := color.darkened(0.4)
	for x in range(w):
		for b in range(min(2, h)):
			img.set_pixel(x, b, border_color)
			img.set_pixel(x, h - 1 - b, border_color)
	for y in range(h):
		for b in range(min(2, w)):
			img.set_pixel(b, y, border_color)
			img.set_pixel(w - 1 - b, y, border_color)
	return ImageTexture.create_from_image(img)
