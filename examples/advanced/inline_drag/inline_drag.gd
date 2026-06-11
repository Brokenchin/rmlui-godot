extends Control
## The drag-and-drop logic lives entirely in inline_drag.rml's <script> block.
## This script only demonstrates the bridge OUT of the document: the block
## declares `signal item_dropped(...)` and any Godot node can connect to it
## through RmlContext.get_document_script().

@onready var _ctx: RmlContext = $RmlContext
@onready var _label: Label = $SignalLabel


func _ready() -> void:
	# Document loads in the RmlContext's own _ready (document_path property);
	# one frame later its <script> block instance exists.
	await get_tree().process_frame

	var doc_script = _ctx.get_document_script()
	if doc_script == null:
		_label.text = "No document script found"
		return

	doc_script.item_dropped.connect(_on_item_dropped)
	_label.text = "Godot side connected — waiting for drops..."


func _on_item_dropped(item_id: String, label: String, total: int) -> void:
	if label == "reset":
		_label.text = "Godot side: counter reset"
	else:
		_label.text = "Godot side received: %s (%s) — %d total" % [label, item_id, total]
