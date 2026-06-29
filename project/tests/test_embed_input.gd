extends SceneTree
## Input routing into embeds (issue #56) — production-critical:
## - hit-testing crosses into the embed subtree (get_element_at_point)
## - hover crosses the boundary (the hover bridge reports the embed element)
## - a REAL forwarded mouse click reaches the embed's own onclick handler

const HOST := "res://tests/fixtures/embed/host.rml"

var _ctx: Node
var _phase := 0
var _fails := 0
var _center := Vector2.ZERO


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_ctx.set_size(Vector2(800, 600))
	create_timer(0.3).timeout.connect(_step)


func _send_motion(p: Vector2) -> void:
	var e := InputEventMouseMotion.new()
	e.position = p
	e.global_position = p
	_ctx.get_viewport().push_input(e, true)


func _send_button(p: Vector2, pressed: bool) -> void:
	var e := InputEventMouseButton.new()
	e.button_index = MOUSE_BUTTON_LEFT
	e.position = p
	e.global_position = p
	e.pressed = pressed
	_ctx.get_viewport().push_input(e, true)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document", HOST)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			var btn = _ctx.call("get_embedded_element", "w1", "w-self-btn")
			_check("embed button resolves", btn != null and btn.is_valid())
			var pos: Vector2 = btn.get_position()
			var sz: Vector2 = btn.get_size()
			_center = pos + sz * 0.5
			print("  [info] embed button pos=%s size=%s center=%s" % [pos, sz, _center])
			_check("embed button laid out (non-zero size)", sz.x > 0 and sz.y > 0)

			# Hit-testing must cross into the embed subtree.
			var hit = _ctx.call("get_element_at_point", _center)
			print("  [info] get_element_at_point(center) -> id=%s tag=%s valid=%s" % [
				str(hit.get_id()), str(hit.get_tag_name()), str(hit.is_valid())])
			_check("get_element_at_point hits inside the embed", hit != null and hit.is_valid())

			# Forward a real move + click at the embed button.
			_send_motion(_center)
			_send_button(_center, true)
			_send_button(_center, false)
			_phase = 2
			create_timer(0.4).timeout.connect(_step)
		2:
			# Click reached the embed's own onclick handler.
			_check("real mouse click reached the embed's handler",
				int(_ctx.get_meta("widget_self_clicked", 0)) >= 1)
			# Hover bridge resolves the embed element under the (now-settled) cursor.
			var hov := str(_ctx.call("get_hovered_element_id"))
			print("  [info] hovered id=%s" % hov)
			_check("hover crosses into embed", hov == "w-self-btn")
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
