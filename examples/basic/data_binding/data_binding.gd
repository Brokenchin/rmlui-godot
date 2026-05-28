extends Control

var _score: int = 0

func _ready() -> void:
	var rml: RmlContext = $RmlContext
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")

	rml.create_data_model("test")
	rml.bind_data_variable("test", "title", "Hello from GDScript!")
	rml.bind_data_variable("test", "score", _score)
	rml.bind_data_event("test", "on_increment", _on_increment)

	rml.load_document("res://addons/rmlui-godot/examples/basic/data_binding/data_binding.rml")


func _on_increment() -> void:
	_score += 1
	var rml: RmlContext = $RmlContext
	rml.set_data_variable("test", "score", _score)
