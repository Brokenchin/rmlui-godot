extends Control

@onready var ctx_a: RmlContext = $ContextA
@onready var ctx_b: RmlContext = $ContextB


func _ready() -> void:
	var font_path := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
	ctx_a.load_font_face(font_path)
	ctx_b.load_font_face(font_path)

	ctx_a.load_document("res://addons/rmlui-godot/examples/advanced/drag_and_drop/drag.rml")
	ctx_b.load_document("res://addons/rmlui-godot/examples/advanced/drag_and_drop/drag.rml")

	await get_tree().process_frame

	for ctx in [ctx_a, ctx_b]:
		for item_id in ["item-1", "item-2", "item-3"]:
			ctx.register_drag_source(item_id, _build_payload)
		ctx.register_drop_target("drop-zone", _on_drop.bind(ctx))
		ctx.rml_drag_started.connect(_on_drag_started.bind(ctx))
		ctx.rml_drop_received.connect(_on_drop_received.bind(ctx))


func _build_payload(element_id: String, _pos: Vector2) -> Dictionary:
	return {
		"element_id": element_id,
		"label": element_id.replace("item-", "Item "),
	}


func _on_drag_started(element_id: String, payload: Dictionary, ctx: RmlContext) -> void:
	var name := "A" if ctx == ctx_a else "B"
	ctx.set_element_inner_rml("status",
		"Dragging %s from Context %s" % [payload.get("label", "?"), name])


func _on_drop(element_id: String, data: Variant, ctx: RmlContext) -> void:
	var name := "A" if ctx == ctx_a else "B"
	ctx.set_element_inner_rml("drop-zone",
		"Received: %s" % data.get("label", "?"))
	ctx.set_element_inner_rml("status",
		"Dropped %s on %s in Context %s" % [data.get("label", "?"), element_id, name])


func _on_drop_received(element_id: String, data: Dictionary, ctx: RmlContext) -> void:
	var name := "A" if ctx == ctx_a else "B"
	print("[DragDemo] Drop received on '%s' in Context %s: %s" % [element_id, name, str(data)])
