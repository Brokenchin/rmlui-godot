extends Control

func _ready() -> void:
	var rml: RmlContext = $RmlContext
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")
	rml.load_document("res://addons/rmlui-godot/examples/basic/hello_world/hello.rml")
