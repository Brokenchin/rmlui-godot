extends Control

func _ready():
	var ctx: RmlContext = $RmlContext
	ctx.document_path = "res://examples/hello_world/hello.rml"
