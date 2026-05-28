extends Control

var _items: Array = ["Sword", "Shield", "Potion"]

func _ready() -> void:
	var rml: RmlContext = $RmlContext
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")

	rml.create_data_model("inventory")
	rml.bind_data_variable("inventory", "item_count", _items.size())
	rml.bind_data_array("inventory", "items", _items.duplicate())
	rml.bind_data_event("inventory", "on_add", _on_add_item)
	rml.bind_data_event("inventory", "on_remove_last", _on_remove_last)
	rml.bind_data_event("inventory", "on_clear", _on_clear)
	rml.bind_data_event("inventory", "on_shuffle", _on_shuffle)

	rml.load_document("res://addons/rmlui-godot/examples/basic/list_binding/list_binding.rml")


func _update_count() -> void:
	var rml: RmlContext = $RmlContext
	var size := rml.get_data_array_size("inventory", "items")
	rml.set_data_variable("inventory", "item_count", size)


func _on_add_item() -> void:
	var rml: RmlContext = $RmlContext
	var new_items := ["Helmet", "Ring", "Amulet", "Boots", "Gloves", "Cape", "Staff", "Bow"]
	var item: String = new_items[randi() % new_items.size()]
	rml.push_data_array_item("inventory", "items", item)
	_update_count()


func _on_remove_last() -> void:
	var rml: RmlContext = $RmlContext
	var size := rml.get_data_array_size("inventory", "items")
	if size > 0:
		rml.remove_data_array_item("inventory", "items", size - 1)
		_update_count()


func _on_clear() -> void:
	var rml: RmlContext = $RmlContext
	rml.clear_data_array("inventory", "items")
	_update_count()


func _on_shuffle() -> void:
	var rml: RmlContext = $RmlContext
	var size := rml.get_data_array_size("inventory", "items")
	if size == 0:
		return
	# Re-populate with a shuffled version
	var shuffled := _items.duplicate()
	var extras := ["Helmet", "Ring", "Amulet", "Boots", "Gloves"]
	for i in range(randi() % 4):
		shuffled.append(extras[randi() % extras.size()])
	shuffled.shuffle()
	rml.set_data_array("inventory", "items", shuffled)
	_update_count()
