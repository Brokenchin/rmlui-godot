extends Control
## Issue #39 visual demo — drag-target events on drop targets, with payload.
##
## Drag "Iron Sword" from the inventory onto the equipment slots:
##   • weapon slot  → rml_drag_entered with the payload; the game checks
##     slot_type, lights it green and pops a compare card (equipped vs dragged)
##   • shield slot  → same event, slot_type mismatch → red highlight
##   • leaving      → rml_drag_left clears highlight + card
##   • rml_drag_over ticks a counter while moving over a slot
##   • dropping     → rml_drop_received equips the item, then the final
##     rml_drag_left clears the highlight
## Everything above is plain GDScript reacting to the signals — no polling.
##
## Run manually (editor: open this scene and Run Current Scene), or self-driving
## with screenshots:  Godot --path . res://tests/visual/drag_over_demo.tscn ++ --auto
## Auto mode performs a REAL native drag: it warps the OS cursor (drag-over
## resolution only follows physical motion) and injects the press/first-motion/
## release, so the ghost, highlights and events are exactly what a player sees.

const SLOT_TYPES := {"weapon-slot": "weapon", "shield-slot": "shield"}
const ITEMS := {
	"inv-sword": {"name": "Iron Sword", "atk": 12, "slot_type": "weapon"},
	"inv-potion": {"name": "Healing Potion", "heal": 50, "slot_type": "potion"},
}
const EQUIPPED := {"name": "Rusty Sword", "atk": 8}

@onready var rml: Node = $Rml

var _log_html := ""
var _overs := 0


func _ready() -> void:
	# Let the deferred document load land before touching elements.
	get_tree().create_timer(0.5).timeout.connect(_setup)


func _setup() -> void:
	for id in ITEMS:
		rml.call("register_drag_source", id, _payload)
	for id in SLOT_TYPES:
		rml.call("register_drop_target", id, _on_drop)
	rml.connect("rml_drag_started", func(id: String, _p: Dictionary) -> void:
		_log("ev-start", "drag_started: %s" % id))
	rml.connect("rml_drag_entered", _on_entered)
	rml.connect("rml_drag_over", _on_over)
	rml.connect("rml_drag_left", _on_left)
	if "--auto" in OS.get_cmdline_user_args():
		_auto()


func _payload(element_id: String, _pos: Vector2) -> Dictionary:
	return ITEMS[element_id].duplicate()


func _on_entered(id: String, data: Dictionary) -> void:
	var valid: bool = data.get("slot_type", "") == SLOT_TYPES.get(id, "")
	rml.call("set_element_class", id, "drag-target", valid)
	rml.call("set_element_class", id, "drag-bad", not valid)
	if id == "weapon-slot" and valid:
		rml.call("set_element_inner_rml", "compare",
			"<div class=\"title\">COMPARE</div>" +
			"<div class=\"name\">%s · ATK %d</div><div class=\"meta\">equipped</div>" %
				[EQUIPPED.name, EQUIPPED.atk] +
			"<div class=\"name cmp-up\">%s · ATK %d (+%d)</div><div class=\"meta\">dragging</div>" %
				[data.name, data.atk, data.atk - EQUIPPED.atk])
		rml.call("set_element_class", "compare", "show", true)
	_log("ev-enter", "drag_entered: %s + %s %s" %
		[id, data.get("name"), "VALID" if valid else "WRONG"])


func _on_over(_id: String, _data: Dictionary) -> void:
	_overs += 1
	rml.call("set_element_inner_rml", "overs", "rml_drag_over events: %d" % _overs)


func _on_left(id: String) -> void:
	rml.call("set_element_class", id, "drag-target", false)
	rml.call("set_element_class", id, "drag-bad", false)
	rml.call("set_element_class", "compare", "show", false)
	_log("ev-left", "drag_left: %s" % id)


func _on_drop(id: String, data: Dictionary) -> void:
	if data.get("slot_type", "") == SLOT_TYPES.get(id, ""):
		rml.call("set_element_inner_rml", "weapon-name", str(data.name))
		rml.call("set_element_inner_rml", "weapon-meta",
			"ATK %d · weapon slot" % int(data.atk))
	_log("ev-drop", "drop_received: %s = %s" % [id, data.get("name")])


func _log(cls: String, text: String) -> void:
	_log_html += "<div class=\"ln %s\">%s</div>" % [cls, text]
	rml.call("set_element_inner_rml", "log", _log_html)
	print("[demo] ", text)


# --- self-driving proof mode (--auto) ---

func _center(id: String) -> Vector2:
	var h = rml.call("get_element_by_id", id)
	return rml.position + Vector2(h.get_position()) + Vector2(h.get_size()) * 0.5


func _button(pos: Vector2, pressed: bool) -> void:
	var ev := InputEventMouseButton.new()
	ev.button_index = MOUSE_BUTTON_LEFT
	ev.pressed = pressed
	ev.position = pos
	ev.global_position = pos
	if pressed:
		ev.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(ev, true)


func _motion_lmb(pos: Vector2, rel: Vector2) -> void:
	# First-motion-with-LMB: what makes the viewport call _get_drag_data and
	# start the native drag (warped real motion carries no button mask — the
	# physical button isn't held). The viewport accumulates `relative` and only
	# attempts the drag past ~10px, so it must be set explicitly.
	var ev := InputEventMouseMotion.new()
	ev.position = pos
	ev.global_position = pos
	ev.relative = rel
	ev.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(ev, true)


func _shot(tag: String) -> void:
	await RenderingServer.frame_post_draw
	var img: Image = get_viewport().get_texture().get_image()
	var path := "res://tests/visual/proof_issue39_%s.png" % tag
	img.save_png(path)
	print("[demo] shot saved: ", path)


func _auto() -> void:
	await get_tree().create_timer(0.4).timeout
	var sword := _center("inv-sword")
	var empty := Vector2(150, 480)   # bare body, no target

	Input.warp_mouse(sword)
	await get_tree().create_timer(0.3).timeout
	_button(sword, true)
	await get_tree().create_timer(0.2).timeout
	var grab := sword + Vector2(26, 6)
	Input.warp_mouse(grab)
	_motion_lmb(grab, Vector2(26, 6))   # native drag starts here (ghost appears)
	await get_tree().create_timer(0.3).timeout
	print("[demo] dragging: ", get_viewport().gui_is_dragging())

	Input.warp_mouse(_center("weapon-slot"))
	await get_tree().create_timer(0.4).timeout
	# Wiggle within the slot so rml_drag_over visibly ticks the counter.
	for off in [Vector2(14, 6), Vector2(-10, -4), Vector2(6, 8)]:
		Input.warp_mouse(_center("weapon-slot") + off)
		await get_tree().create_timer(0.15).timeout
	await _shot("1_valid_slot_highlight")

	Input.warp_mouse(empty)
	await get_tree().create_timer(0.5).timeout
	await _shot("2_left_cleared")

	Input.warp_mouse(_center("shield-slot"))
	await get_tree().create_timer(0.5).timeout
	await _shot("3_wrong_slot_red")

	Input.warp_mouse(_center("weapon-slot"))
	await get_tree().create_timer(0.5).timeout
	_button(_center("weapon-slot"), false)   # release = drop
	await get_tree().create_timer(0.5).timeout
	await _shot("4_dropped_equipped")

	print("[demo] over-event count: ", _overs)
	print("[demo] DONE")
	await get_tree().create_timer(0.3).timeout
	get_tree().quit(0)
