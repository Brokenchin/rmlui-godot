extends Control

func _ready():
	var ctx: RmlContext = $RmlContext
	ctx.document_path = "res://examples/drag_and_drop/drag.rml"

	ctx.rml_drop_received.connect(_on_drop)

func _on_drop(element_id: String, data: Dictionary):
	print("Dropped on: ", element_id, " data: ", data)
