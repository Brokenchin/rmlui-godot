class_name ParityGodotSlot_GD extends PanelContainer

var slot_type: String = ""
var slot_key: String = ""
var item_data: Dictionary = {}
var placeholder_text: String = ""
var on_swap: Callable

var _default_style: StyleBox = null
var _hover_style: StyleBox = null
var _drop_hover_style: StyleBox = null
var _icon_node: TextureRect = null
var _name_label: Label = null


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	clip_contents = true
	_icon_node = get_child(0) as TextureRect
	_build_styles()
	add_theme_stylebox_override("panel", _default_style)
	mouse_entered.connect(_on_mouse_entered)
	mouse_exited.connect(_on_mouse_exited)

	var label_bg := ColorRect.new()
	label_bg.color = Color(0, 0, 0, 0.67)
	label_bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	label_bg.anchor_left = 0.0
	label_bg.anchor_right = 1.0
	label_bg.anchor_top = 1.0
	label_bg.anchor_bottom = 1.0
	label_bg.offset_top = -14
	label_bg.offset_bottom = 0
	_icon_node.add_child(label_bg)

	_name_label = Label.new()
	_name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_name_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_name_label.add_theme_font_size_override("font_size", 8)
	_name_label.add_theme_color_override("font_color", Color.WHITE)
	_name_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_name_label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	label_bg.add_child(_name_label)


func get_unified_key() -> String:
	return "%s-%s" % [slot_type, slot_key]


func set_item(data: Dictionary) -> void:
	item_data = data
	if _icon_node and data.has("icon_texture"):
		_icon_node.texture = data["icon_texture"]
		_icon_node.visible = true
	if _name_label:
		_name_label.text = data.get("name", "")


func clear_item() -> void:
	item_data = {}
	if _icon_node:
		_icon_node.texture = null
		_icon_node.visible = false
	if _name_label:
		_name_label.text = placeholder_text


func has_item() -> bool:
	return not item_data.is_empty()


func _get_drag_data(_at_position: Vector2) -> Variant:
	if item_data.is_empty():
		return null
	modulate = Color(1, 1, 1, 0.35)
	var preview := PanelContainer.new()
	preview.custom_minimum_size = Vector2(40, 40)
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.06, 0.1, 0.18, 0.9)
	style.border_color = Color(0.91, 0.27, 0.37)
	style.set_border_width_all(1)
	preview.add_theme_stylebox_override("panel", style)
	var tex_rect := TextureRect.new()
	tex_rect.texture = item_data.get("icon_texture")
	tex_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	tex_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	preview.add_child(tex_rect)
	set_drag_preview(preview)
	return {
		"slot_type": slot_type,
		"slot_key": slot_key,
		"unified_key": get_unified_key(),
		"item_data": item_data,
		"source": self,
	}


func _can_drop_data(_at_position: Vector2, data: Variant) -> bool:
	if data is not Dictionary:
		return false
	if data.get("source") == self:
		return false
	if data.has("unified_key") or data.has("_rml_element_id"):
		return true
	return false


func _drop_data(_at_position: Vector2, data: Variant) -> void:
	if data is not Dictionary:
		return
	if on_swap.is_valid():
		on_swap.call(get_unified_key(), data)
	else:
		_fallback_swap(data)


func _fallback_swap(data: Dictionary) -> void:
	var source: ParityGodotSlot_GD = data.get("source")
	if source == null:
		return
	var my_item := item_data.duplicate()
	var their_item: Dictionary = data.get("item_data", {})
	if not their_item.is_empty():
		set_item(their_item)
	else:
		clear_item()
	if not my_item.is_empty():
		source.set_item(my_item)
	else:
		source.clear_item()


func _notification(what: int) -> void:
	if what == NOTIFICATION_DRAG_END:
		modulate = Color.WHITE


func _build_styles() -> void:
	_default_style = StyleBoxFlat.new()
	_default_style.bg_color = Color(0.06, 0.1, 0.18)
	_default_style.border_color = Color(0.06, 0.2, 0.37)
	_default_style.set_border_width_all(1)
	_default_style.set_corner_radius_all(2)

	_hover_style = _default_style.duplicate()
	_hover_style.border_color = Color(0.91, 0.27, 0.37)

	_drop_hover_style = _default_style.duplicate()
	_drop_hover_style.border_color = Color(0.27, 1.0, 0.53)
	_drop_hover_style.bg_color = Color(0.1, 0.35, 0.23)


func _on_mouse_entered() -> void:
	add_theme_stylebox_override("panel", _hover_style)


func _on_mouse_exited() -> void:
	add_theme_stylebox_override("panel", _default_style)
