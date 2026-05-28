extends Control

var _rml: RmlContext
var _sprite_names: Array = [
	"fruit-apple", "fruit-banana", "fruit-cherry", "fruit-grape",
	"fruit-lemon", "fruit-strawberry", "fruit-kiwi", "fruit-blueberry",
	"fruit-orange", "fruit-peach", "fruit-pear", "fruit-plum",
	"fruit-watermelon", "fruit-mango", "fruit-pineapple", "fruit-coconut",
]
var _cycle_timer: float = 0.0

func _ready() -> void:
	_rml = $RmlContext
	_rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	_rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")

	# Set up data model for data-bound sprites
	_rml.create_data_model("inventory")
	_rml.bind_data_variable("inventory", "selected_item", "item-sword")
	_rml.bind_data_array("inventory", "items", _sprite_names.duplicate())

	_rml.load_document("res://addons/rmlui-godot/examples/advanced/textures/textures.rml")


func _process(delta: float) -> void:
	_cycle_timer += delta
	if _cycle_timer >= 2.0:
		_cycle_timer = 0.0
		_cycle_inventory()


func _cycle_inventory() -> void:
	# Shuffle the sprite list to demonstrate dynamic data binding updates
	var shuffled := _sprite_names.duplicate()
	shuffled.shuffle()

	# Randomly vary the count
	var count := randi_range(3, _sprite_names.size())
	var subset: Array = []
	for i in range(count):
		subset.append(shuffled[i])

	_rml.set_data_array("inventory", "items", subset)
	_rml.set_data_variable("inventory", "selected_item", subset[0])
	_rml.dirty_all_variables("inventory")
