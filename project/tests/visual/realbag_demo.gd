extends Control
## Faithful repro of the game bag using the REAL theme.rcss + exact slot HTML
## (rml_slots.gd) + generated icons + real dp sizes/dp_ratio, embedded.

const N := 120

@onready var standalone: Node = $Standalone
@onready var embedded: Node = $Embedded


func _ready() -> void:
	_register_icon(standalone)
	_register_icon(embedded)
	get_tree().create_timer(0.6).timeout.connect(_fill)


func _register_icon(ctx: Node) -> void:
	var img := Image.create(32, 32, false, Image.FORMAT_RGBA8)
	img.fill(Color(0.9, 0.7, 0.15, 1.0))
	for k in 32:
		img.set_pixel(k, k, Color(0.2, 0.8, 1.0))
		img.set_pixel(31 - k, k, Color(1.0, 0.3, 0.4))
	ctx.call("register_texture", "texture://icon", ImageTexture.create_from_image(img))


func _slots() -> String:
	var s := "<div class=\"slot-grid grid-left\">"
	for i in N:
		s += "<div id=\"slot_%d\" class=\"slot\"><div class=\"slot-box rarity-uncommon\"><img class=\"slot-icon\" src=\"texture://icon\"/><div class=\"slot-fx fx-uncommon\"></div></div></div>" % i
	s += "</div>"
	return s


func _fill() -> void:
	standalone.call("set_element_inner_rml", "grid", _slots())
	embedded.call("set_element_inner_rml", "grid", _slots())
	get_tree().create_timer(0.4).timeout.connect(_scroll)


func _wheel(ctx: Node, local_pos: Vector2) -> void:
	var g: Vector2 = ctx.position + local_pos
	var e := InputEventMouseButton.new()
	e.button_index = MOUSE_BUTTON_WHEEL_DOWN
	e.position = g
	e.global_position = g
	e.pressed = true
	e.factor = 1.0
	ctx.get_viewport().push_input(e, true)


func _scroll() -> void:
	for n in 24:
		_wheel(standalone, Vector2(80, 180))
		_wheel(embedded, Vector2(80, 380))
