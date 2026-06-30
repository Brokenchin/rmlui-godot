extends SceneTree
## Issue #61 — an overflow element must clip its content for ALL interaction paths,
## standalone AND embedded. Same widget (clip_demo_panel.rml: a flex-column doc with
## an `overflow-y: auto; flex-grow: 1` scroll viewport over a tall slot grid) loaded
## two ways: standalone, and embedded via <embed-doc>.
##
## Two clip-consuming paths are checked at a slot laid out PAST the viewport:
##   (A) RmlUi native hit-testing  (get_element_at_point) — already clips.
##   (B) the addon drag/drop bridge (_point_in_element, used by _can_drop_data /
##       _get_drag_data / _drop_data) — this is the #61 leak: it tested only the
##       element's own border box, so off-viewport slots stayed droppable.

const PANEL := "res://tests/fixtures/embed/clip_demo_panel.rml"
const HOST := "res://tests/fixtures/embed/clip_demo_host.rml"

const IN_VIEW := "slot_0"      # first row — inside the viewport
const OFF_VIEW := "slot_39"    # last row — well past the viewport bottom

var _standalone: Node
var _embedded: Node
var _phase := 0
var _fails := 0


func _initialize() -> void:
	_standalone = ClassDB.instantiate(&"RmlContext")
	root.add_child(_standalone)
	_standalone.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_standalone.set_size(Vector2(900, 700))

	_embedded = ClassDB.instantiate(&"RmlContext")
	root.add_child(_embedded)
	_embedded.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_embedded.set_size(Vector2(900, 700))

	create_timer(0.4).timeout.connect(_step)


func _noop(_a = null, _b = null) -> void:
	pass


func _center(ctx: Node, id: String) -> Vector2:
	var h = ctx.call("get_element_by_id", id)
	if h == null or not h.is_valid():
		return Vector2(-1, -1)
	return h.get_position() + h.get_size() * 0.5


func _probe(ctx: Node, mode: String) -> void:
	# Register both slots as addon drop targets so _can_drop_data resolves them.
	ctx.call("register_drop_target", IN_VIEW, Callable(self, "_noop"))
	ctx.call("register_drop_target", OFF_VIEW, Callable(self, "_noop"))

	var c_in := _center(ctx, IN_VIEW)
	var c_off := _center(ctx, OFF_VIEW)

	# The off-view slot's OWN border box contains the probe point — so the pre-#61
	# box-only _point_in_element WOULD have accepted it. Proves the clip (not a miss)
	# is what rejects the drop below, i.e. the test actually exercises the fix.
	var off_h = ctx.call("get_element_by_id", OFF_VIEW)
	_check("%s: off-view box contains the probe point (clip, not a miss, rejects it)" % mode,
		Rect2(off_h.get_position(), off_h.get_size()).has_point(c_off))

	# (A) RmlUi native hit-testing.
	var hit_in := str(ctx.call("get_element_at_point", c_in).get_id())
	var hit_off_h = ctx.call("get_element_at_point", c_off)
	var hit_off := str(hit_off_h.get_id()) if (hit_off_h != null and hit_off_h.is_valid()) else "<none>"
	_check("%s/native: in-view %s is hit-testable" % [mode, IN_VIEW], hit_in == IN_VIEW)
	_check("%s/native: off-view %s is CLIPPED" % [mode, OFF_VIEW], hit_off != OFF_VIEW)

	# (B) addon drag/drop bridge (the #61 path). get_drop_target_at_point shares the
	# exact resolution (_drop_target_at -> _point_in_element) used by the Godot
	# drag/drop virtuals _can_drop_data / _drop_data / _get_drag_data.
	var drop_in := str(ctx.call("get_drop_target_at_point", c_in))
	var drop_off := str(ctx.call("get_drop_target_at_point", c_off))
	print("  [info] %-10s in=%s(target=%s) off=%s(target=%s)" % [mode, c_in, drop_in, c_off, drop_off])
	_check("%s/drag-drop: in-view %s accepts a drop" % [mode, IN_VIEW], drop_in == IN_VIEW)
	_check("%s/drag-drop: off-view %s is CLIPPED from drops" % [mode, OFF_VIEW], drop_off != OFF_VIEW)


func _step() -> void:
	match _phase:
		0:
			_standalone.call("load_document", PANEL)
			_embedded.call("load_document", HOST)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			print("--- STANDALONE (own context) ---")
			_probe(_standalone, "standalone")
			print("--- EMBEDDED (<embed-doc>) ---")
			_probe(_embedded, "embedded")
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
