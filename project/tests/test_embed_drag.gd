extends SceneTree
## Drag-and-drop bridge across embeds (issue #56) — the inventory use case.
##
## Godot's native drag state machine can't be driven by synthetic input, so the
## actual mouse drag is verified by the live example (examples/advanced/embed_drag,
## run it and drag with the mouse). What IS automatable — and is the only part
## specific to embeds — is that the addon's gd_drag bridge can resolve a drag
## source and a drop target that live in two DIFFERENT embedded sub-documents.
## The bridge (_get_drag_data / _drop_data) does exactly: _find_element(id) +
## hit-test at the element's position. This test exercises both for embed
## elements, so a real drag (which calls those with the same positions) works.

const HOST := "res://tests/fixtures/embed/drag_host.rml"

var _ctx: Node
var _phase := 0
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_ctx.set_size(Vector2(800, 600))
	create_timer(0.3).timeout.connect(_step)


func _payload(_element_id: String, _at_pos: Vector2) -> Dictionary:
	return {"item": "sword"}

func _on_drop(_element_id: String, _data) -> void:
	pass


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document", HOST)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			# Register a source inside embed "src" and a target inside embed "dst".
			_ctx.call("register_drag_source", "drag-item", _payload)
			_ctx.call("register_drop_target", "drop-slot", _on_drop)

			var item = _ctx.call("get_element_by_id", "drag-item")
			var slot = _ctx.call("get_element_by_id", "drop-slot")
			var ca: Vector2 = item.get_position() + item.get_size() * 0.5
			var cb: Vector2 = slot.get_position() + slot.get_size() * 0.5
			print("  [info] source center=%s  target center=%s" % [ca, cb])

			# (1) resolve by id across embeds — what _find_element does in the bridge.
			_check("drag source resolves by id (embed A)", item != null and item.is_valid())
			_check("drop target resolves by id (embed B)", slot != null and slot.is_valid())
			_check("source and target live in different embeds", ca.x != cb.x)

			# (2) hit-test at the element's position across embeds — what
			# _point_in_element / GetElementAtPoint do in _get_drag_data / _drop_data.
			_check("drag source is hit-tested at its position (embed A)",
				str(_ctx.call("get_element_at_point", ca).get_id()) == "drag-item")
			_check("drop target is hit-tested at its position (embed B)",
				str(_ctx.call("get_element_at_point", cb).get_id()) == "drop-slot")

			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
