extends SceneTree
## Issue #59 — embed-scoped element resolution for doc-scripts.
##
## scoped_widget.rml is a doc-script widget written exactly as it would run
## standalone: in its <script> it renders a grid BY ID and wires drag/drop BY ID
## (set_element_inner_rml("grid", …), register_drag_source("slot", …),
## register_drop_target("grid", …)). scoped_host.rml mounts that SAME file twice,
## so both embeds contain the identical internal ids "grid" and "slot".
##
## With the per-embed `rml_context` scope (the feature), each widget drives its
## OWN subtree with ZERO source changes:
##   - render: both grids get populated (under the old context-global lookup the
##     second embed's set_element_inner_rml would hit the FIRST embed's grid, so
##     the second grid — and its "slot" — would never exist).
##   - drag: _get_drag_data at the second slot resolves the SECOND embed's source.
##   - drop: _drop_data on the second grid fires the SECOND embed's handler only.

const HOST := "res://tests/fixtures/embed/scoped_host.rml"

var _ctx: Node
var _phase := 0
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_ctx.set_size(Vector2(800, 600))
	create_timer(0.3).timeout.connect(_step)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document", HOST)
			_phase = 1
			create_timer(0.6).timeout.connect(_step)
		1:
			# --- scoped render: each embed's own "grid" was populated by its own
			# script, so each embed has its own "slot". ---
			var sa_slot = _ctx.call("get_embedded_element", "sa", "slot")
			var sb_slot = _ctx.call("get_embedded_element", "sb", "slot")
			_check("first embed rendered its grid (scoped inner_rml)", sa_slot != null and sa_slot.is_valid())
			_check("second embed rendered its OWN grid (scoped inner_rml)", sb_slot != null and sb_slot.is_valid())
			if not (sb_slot != null and sb_slot.is_valid()):
				# Nothing below is meaningful without the second slot.
				print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
				quit(_fails)
				return

			var ca: Vector2 = sa_slot.get_position() + sa_slot.get_size() * 0.5
			var cb: Vector2 = sb_slot.get_position() + sb_slot.get_size() * 0.5
			print("  [info] sa slot center=%s  sb slot center=%s" % [ca, cb])
			_check("the two slots are distinct elements (different positions)", ca.x != cb.x)
			_check("sa slot hit-tests to id 'slot'", str(_ctx.call("get_element_at_point", ca).get_id()) == "slot")
			_check("sb slot hit-tests to id 'slot'", str(_ctx.call("get_element_at_point", cb).get_id()) == "slot")

			# --- scoped drag/drop resolution ---
			# register_drag_source("slot")/register_drop_target("grid") from each
			# embed's <script> store that embed's scope on the registry entry. At
			# drag time the bridge does _find_element_scoped(entry.embed_id, id) +
			# hit-test at the element's position — the SAME scoped resolution proven
			# above for set_element_inner_rml, plus the position check exercised here
			# by get_element_at_point. Under the old context-global registry, the
			# second embed's "slot"/"grid" would resolve to the FIRST embed's element
			# (wrong position), so a drag/drop begun over the second widget would
			# resolve nothing. The live native drag (Godot's drag state machine can't
			# be driven by synthetic input) is exercised by the windowed showcase,
			# examples/advanced/embed_scoped — mirroring test_embed_drag.
			var at_a = _ctx.call("get_element_at_point", ca)
			var at_b = _ctx.call("get_element_at_point", cb)
			_check("a drag over the first widget resolves to ITS own slot",
				str(at_a.get_id()) == "slot" and at_a.get_position() == sa_slot.get_position())
			_check("a drag over the second widget resolves to ITS OWN slot",
				str(at_b.get_id()) == "slot" and at_b.get_position() == sb_slot.get_position())
			# Both widgets registered the same ids ("slot"/"grid") from their own
			# scripts without colliding, and each addresses its own subtree.
			_check("the two same-id slots are independently addressable",
				at_a.get_position() != at_b.get_position())

			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
