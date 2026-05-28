extends Control

const FONT_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const DOC_PATH := "res://addons/rmlui-godot/examples/stress/scissor_limits/scissor_limits.rml"

@onready var rml: RmlContext = $RmlContext


func _ready() -> void:
	rml.load_font_face(FONT_PATH)
	rml.load_document(DOC_PATH)

	await get_tree().process_frame

	_setup_textures()


func _setup_textures() -> void:
	var big_tex := _make_checkerboard(80, 80, Color(0.9, 0.2, 0.2), Color(0.7, 0.1, 0.1), 8)
	rml.register_texture("tex://checker", big_tex)
	rml.set_element_attribute("t4-img", "src", "tex://checker")

	var slot_tex := _make_color_texture(Color(0.8, 0.6, 0.1), 48, 48)
	rml.register_texture("tex://slot", slot_tex)

	for id in ["t5-s1", "t5-s2", "t5-s3"]:
		var inner := '<img class="mini-slot-icon" src="tex://slot" />'
		rml.set_element_inner_rml(id, inner)


func _make_color_texture(color: Color, w: int, h: int) -> ImageTexture:
	var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
	img.fill(color)
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


func _make_checkerboard(w: int, h: int, c1: Color, c2: Color, cell: int) -> ImageTexture:
	var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
	for y in range(h):
		for x in range(w):
			var checker := ((x / cell) + (y / cell)) % 2 == 0
			img.set_pixel(x, y, c1 if checker else c2)
	return ImageTexture.create_from_image(img)
