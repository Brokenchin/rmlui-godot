extends Control

const MOCK_ITEMS: Array[Dictionary] = [
	{"name": "Sword", "color": Color.FIREBRICK, "equip": "weapon"},
	{"name": "Shield", "color": Color.STEEL_BLUE, "equip": "shield"},
	{"name": "Helmet", "color": Color.DARK_GOLDENROD, "equip": "head"},
	{"name": "Mail", "color": Color.DIM_GRAY, "equip": "chest"},
	{"name": "Greaves", "color": Color.SADDLE_BROWN, "equip": "legs"},
	{"name": "Boots", "color": Color.DARK_SLATE_GRAY, "equip": "feet"},
	{"name": "Potion", "color": Color.GREEN, "equip": ""},
	{"name": "Gem", "color": Color.DARK_ORCHID, "equip": ""},
	{"name": "Ring", "color": Color.GOLD, "equip": ""},
	{"name": "Scroll", "color": Color.WHEAT, "equip": ""},
]

const EQUIP_KEYS: Array[String] = ["head", "weapon", "chest", "shield", "legs", "feet"]
const FONT_PATH_REGULAR := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const FONT_PATH_MEDIUM := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const RML_DOC_PATH := "res://addons/rmlui-godot/examples/showcase/visual_parity/visual_parity_equip_inv.rml"

var _slot_data: Dictionary = {}
var _godot_slot_map: Dictionary = {}
var _rml_id_map: Dictionary = {}
var _rml_to_unified: Dictionary = {}
var _all_unified_keys: Array[String] = []

var _rml: RmlContext = null
var _item_textures: Dictionary = {}
var _godot_status: Label = null


func _ready() -> void:
	_generate_textures()
	_init_slot_data()
	_apply_shared_font()
	_setup_godot_slots()
	_setup_rml()


func _generate_textures() -> void:
	for item in MOCK_ITEMS:
		var img := Image.create(32, 32, false, Image.FORMAT_RGBA8)
		var col: Color = item["color"]
		img.fill(col)
		for x in range(32):
			img.set_pixel(x, 0, col.darkened(0.4))
			img.set_pixel(x, 31, col.darkened(0.4))
		for y in range(32):
			img.set_pixel(0, y, col.darkened(0.4))
			img.set_pixel(31, y, col.darkened(0.4))
		var tex := ImageTexture.create_from_image(img)
		_item_textures[item["name"]] = tex


func _init_slot_data() -> void:
	for key in EQUIP_KEYS:
		var unified := "equip-%s" % key
		_all_unified_keys.append(unified)
		_rml_id_map[unified] = "rml-eq-%s" % key
		_rml_to_unified["rml-eq-%s" % key] = unified
		_slot_data[unified] = {}

	for i in range(16):
		var unified := "inv-%d" % i
		_all_unified_keys.append(unified)
		_rml_id_map[unified] = "rml-inv-%d" % i
		_rml_to_unified["rml-inv-%d" % i] = unified
		_slot_data[unified] = {}

	for item in MOCK_ITEMS:
		var equip_key: String = item["equip"]
		if equip_key.is_empty():
			continue
		_slot_data["equip-%s" % equip_key] = _make_item_data(item)

	var inv_index := 0
	for item in MOCK_ITEMS:
		if not item["equip"].is_empty():
			continue
		if inv_index >= 16:
			break
		_slot_data["inv-%d" % inv_index] = _make_item_data(item)
		inv_index += 1


func _make_item_data(item: Dictionary) -> Dictionary:
	return {
		"name": item["name"],
		"color": item["color"],
		"icon_texture": _item_textures.get(item["name"]),
	}


func _apply_shared_font() -> void:
	var font := load(FONT_PATH_MEDIUM) as Font
	if font:
		var t := Theme.new()
		t.default_font = font
		$Columns/GodotSide.theme = t
	_godot_status = $Columns/GodotSide/GodotVBox/GodotStatus


# --- Godot Setup ---

func _setup_godot_slots() -> void:
	for key in EQUIP_KEYS:
		var node_name := "Eq" + key.capitalize()
		var slot := _find_slot(node_name)
		if slot:
			var unified := "equip-%s" % key
			slot.slot_type = "equip"
			slot.slot_key = key
			slot.placeholder_text = key.capitalize()
			slot.on_swap = _on_godot_swap
			_godot_slot_map[unified] = slot

	for i in range(16):
		var node_name := "Inv%d" % i
		var slot := _find_slot(node_name)
		if slot:
			var unified := "inv-%d" % i
			slot.slot_type = "inv"
			slot.slot_key = str(i)
			slot.on_swap = _on_godot_swap
			_godot_slot_map[unified] = slot

	for unified_key in _all_unified_keys:
		_refresh_godot_slot(unified_key)


func _find_slot(node_name: String) -> ParityGodotSlot_GD:
	var nodes := _find_children_by_name(self, node_name)
	if nodes.size() > 0:
		return nodes[0] as ParityGodotSlot_GD
	return null


func _find_children_by_name(parent: Node, target_name: String) -> Array[Node]:
	var result: Array[Node] = []
	if parent.name == target_name:
		result.append(parent)
		return result
	for child in parent.get_children():
		result.append_array(_find_children_by_name(child, target_name))
	return result


# --- RmlUI Setup ---

func _setup_rml() -> void:
	_rml = $Columns/RmlSide/RmlContext as RmlContext
	if _rml == null:
		push_warning("RmlContext node not found")
		return

	#_rml.load_font_face(FONT_PATH_REGULAR)
	_rml.load_font_face(FONT_PATH_MEDIUM)
	_rml.load_document(RML_DOC_PATH)

	_populate_rml_from_store()

	await get_tree().process_frame

	_register_rml_drag_sources()
	_rml.rml_drag_started.connect(_on_rml_drag_started)


func _populate_rml_from_store() -> void:
	for key in EQUIP_KEYS:
		var unified := "equip-%s" % key
		var rml_id: String = _rml_id_map[unified]
		var data: Dictionary = _slot_data[unified]
		_render_rml_slot(rml_id, data, true, key)

	var grid_rml := ""
	for i in range(16):
		var unified := "inv-%d" % i
		var rml_id: String = _rml_id_map[unified]
		var data: Dictionary = _slot_data[unified]
		var content := _build_rml_slot_content(data, false, str(i))
		grid_rml += '<div class="inv-slot" id="%s">%s</div>' % [rml_id, content]

	_rml.set_element_inner_rml("rml-inv-grid", grid_rml)


func _build_rml_slot_content(data: Dictionary, _is_equip: bool, _label: String) -> String:
	if data.is_empty():
		return ""

	var tex_key := _register_rml_texture(data)
	var img_tag := ""
	if not tex_key.is_empty():
		img_tag = '<img class="slot-icon" src="%s" />' % tex_key
	return '<div class="slot-cell">%s<div class="slot-name">%s</div></div>' % [img_tag, data.get("name", "?")]


func _render_rml_slot(rml_id: String, data: Dictionary, is_equip: bool, label: String) -> void:
	_rml.set_element_inner_rml(rml_id, _build_rml_slot_content(data, is_equip, label))


func _register_rml_texture(data: Dictionary) -> String:
	var tex: ImageTexture = _item_textures.get(data.get("name", ""))
	if tex == null:
		return ""
	var key := "tex://%s" % data["name"]
	_rml.register_texture(key, tex)
	return key


# --- RmlUI Drag via C++ native API ---

func _register_rml_drag_sources() -> void:
	for unified_key in _all_unified_keys:
		var rml_id: String = _rml_id_map[unified_key]
		_rml.register_drag_source(rml_id, _build_rml_payload)
		_rml.register_drop_target(rml_id, _on_rml_drop)


func _build_rml_payload(element_id: String, _pos: Vector2) -> Dictionary:
	var unified_key: String = _rml_to_unified.get(element_id, "")
	var data: Dictionary = _slot_data.get(unified_key, {})
	return {
		"_rml_element_id": element_id,
		"unified_key": unified_key,
		"item_name": data.get("name", ""),
		"item_color": data.get("color", Color.WHITE),
	}


func _on_rml_drag_started(element_id: String, _payload: Dictionary) -> void:
	_rml.set_element_inner_rml("rml-status", "Dragging: %s" % element_id)


# --- Unified Swap Logic ---

func _resolve_unified_key(data: Dictionary) -> String:
	if data.has("unified_key") and not (data["unified_key"] as String).is_empty():
		return data["unified_key"]
	if data.has("_rml_element_id"):
		return _rml_to_unified.get(data["_rml_element_id"], "")
	if data.has("slot_type") and data.has("slot_key"):
		return "%s-%s" % [data["slot_type"], data["slot_key"]]
	return ""


func _swap_slots(key_a: String, key_b: String) -> void:
	if key_a == key_b or key_a.is_empty() or key_b.is_empty():
		return

	var data_a: Dictionary = _slot_data.get(key_a, {}).duplicate()
	var data_b: Dictionary = _slot_data.get(key_b, {}).duplicate()

	_slot_data[key_a] = data_b
	_slot_data[key_b] = data_a

	_refresh_slot(key_a)
	_refresh_slot(key_b)

	var msg := "Swapped: %s | %s" % [key_a, key_b]
	if _rml:
		_rml.set_element_inner_rml("rml-status", msg)
	if _godot_status:
		_godot_status.text = msg


func _refresh_slot(unified_key: String) -> void:
	_refresh_godot_slot(unified_key)
	_refresh_rml_slot(unified_key)


func _refresh_godot_slot(unified_key: String) -> void:
	var slot: ParityGodotSlot_GD = _godot_slot_map.get(unified_key)
	if slot == null:
		return
	var data: Dictionary = _slot_data.get(unified_key, {})
	if data.is_empty():
		slot.clear_item()
	else:
		slot.set_item(data)


func _refresh_rml_slot(unified_key: String) -> void:
	if _rml == null:
		return
	var rml_id: String = _rml_id_map.get(unified_key, "")
	if rml_id.is_empty():
		return
	var data: Dictionary = _slot_data.get(unified_key, {})
	var is_equip := unified_key.begins_with("equip-")
	var label := unified_key.split("-", true, 1)[1] if "-" in unified_key else ""
	_render_rml_slot(rml_id, data, is_equip, label)


# --- Drop Handlers ---

func _on_godot_swap(target_unified_key: String, data: Dictionary) -> void:
	var source_key := _resolve_unified_key(data)
	_swap_slots(source_key, target_unified_key)


func _on_rml_drop(target_element_id: String, data: Variant) -> void:
	if data is not Dictionary:
		return
	var target_key: String = _rml_to_unified.get(target_element_id, "")
	var source_key := _resolve_unified_key(data)
	_swap_slots(source_key, target_key)
