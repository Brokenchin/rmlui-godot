extends SceneTree
## Exercises the drag-target events (issue #39): rml_drag_entered /
## rml_drag_over / rml_drag_left + get_drag_over_target(). A real viewport drag
## is started with force_drag (bypassing _get_drag_data) and the cursor is moved
## with Input.warp_mouse — NOT pushed events: during a drag the viewport resolves
## the control under the cursor from its mouse-over bookkeeping
## (gui.target_control), which only physical mouse motion updates. Warping the
## OS cursor drives the exact native pipeline: motion → _can_drop_data on the
## control under the cursor (the bridge's event feed, payload via
## gui_get_drag_data) → drop on button release. The drop is a pushed left-button
## release at the warped position, verifying rml_drop_received still fires and
## the final rml_drag_left lands AFTER it (highlight cleanup ordering).
##
## Windowed (warp_mouse is a no-op headless) and the window must keep focus for
## the run. rml_drag_over is asserted by count/payload, not position in the
## event order: the OS may deliver more than one motion per warp.

const DOC := """<rml>
<head>
<style>
body { display: block; width: 100%; height: 100%; background-color: #202020; }
#slot-a, #slot-b { display: block; width: 120px; height: 50px; margin: 20px; }
#slot-a { background-color: #3a4254; }
#slot-b { background-color: #445a44; }
</style>
</head>
<body>
	<div id="slot-a"/>
	<div id="slot-b"/>
</body>
</rml>"""

const PAYLOAD := {"item": "sword", "slot_type": "weapon"}

const IN_A := Vector2(80, 45)        # inside #slot-a
const IN_A2 := Vector2(100, 55)      # still inside #slot-a, different point
const IN_B := Vector2(80, 125)       # inside #slot-b
const EMPTY := Vector2(400, 300)     # bare body — no registered target

var _ctx: Node
var _phase := 0
var _fails := 0

var _events: Array = []       # ordered ["entered:id", "left:id", "drop:id"]
var _entered_data: Array = [] # payload of each rml_drag_entered
var _overs: Array = []        # [id, payload] of each rml_drag_over


func _initialize() -> void:
	root.grab_focus()
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_ctx.set_size(Vector2(800, 600))
	_ctx.connect("rml_drag_entered", func(id: String, data: Dictionary) -> void:
		_events.append("entered:" + id)
		_entered_data.append(data))
	_ctx.connect("rml_drag_over", func(id: String, data: Dictionary) -> void:
		_overs.append([id, data]))
	_ctx.connect("rml_drag_left", func(id: String) -> void:
		_events.append("left:" + id))
	_ctx.connect("rml_drop_received", func(id: String, _data: Dictionary) -> void:
		_events.append("drop:" + id))
	create_timer(0.3).timeout.connect(_step)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document_from_string", DOC, "memory://drag_over_events")
			_advance(0.5)
		1:
			_ctx.call("register_drop_target", "slot-a")
			_ctx.call("register_drop_target", "slot-b")
			# Park the cursor over target-free space, then start a viewport drag.
			Input.warp_mouse(EMPTY)
			_ctx.force_drag(PAYLOAD, null)
			_advance(0.3)
		2:
			_check("drag away from targets fires nothing",
				_events.is_empty() and _overs.is_empty())
			_check("get_drag_over_target() empty away from targets",
				_ctx.call("get_drag_over_target") == "")
			Input.warp_mouse(IN_A)
			_advance(0.3)
		3:
			_check("entering slot-a mid-drag emitted entered(slot-a)",
				_events == ["entered:slot-a"])
			_check("entered carries the drag payload",
				_entered_data.size() == 1 and _entered_data[0].get("item") == "sword")
			_check("get_drag_over_target() reports slot-a during the drag",
				_ctx.call("get_drag_over_target") == "slot-a")
			# Move within slot-a: over (with payload), but no enter/leave.
			Input.warp_mouse(IN_A2)
			_advance(0.3)
		4:
			_check("moving within slot-a emitted over(slot-a), no enter/leave",
				_events == ["entered:slot-a"] and _overs.size() >= 1
				and _overs.all(func(o): return o[0] == "slot-a"))
			_check("over carries the drag payload",
				_overs.size() >= 1 and _overs[0][1].get("slot_type") == "weapon")
			# Cross straight to slot-b: leave a, enter b.
			Input.warp_mouse(IN_B)
			_advance(0.3)
		5:
			_check("crossing to slot-b emitted left(slot-a) then entered(slot-b)",
				_events.slice(1) == ["left:slot-a", "entered:slot-b"])
			# Out to target-free space: leave b, no enter.
			Input.warp_mouse(EMPTY)
			_advance(0.3)
		6:
			_check("moving off the targets emitted left(slot-b)",
				_events.slice(3) == ["left:slot-b"])
			# Back onto slot-b and DROP there (left-button release ends the drag).
			Input.warp_mouse(IN_B)
			_advance(0.3)
		7:
			_check("re-entering slot-b emitted entered(slot-b)",
				_events.slice(4) == ["entered:slot-b"])
			_push_release(IN_B)
			_advance(0.3)
		8:
			_check("drop fired rml_drop_received(slot-b) then the final left(slot-b)",
				_events.slice(5) == ["drop:slot-b", "left:slot-b"])
			_check("tracking cleared after the drag ended",
				_ctx.call("get_drag_over_target") == "")
			_check("no spurious extra events", _events.size() == 7)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _advance(delay: float) -> void:
	_phase += 1
	create_timer(delay).timeout.connect(_step)


func _push_release(pos: Vector2) -> void:
	var ev := InputEventMouseButton.new()
	ev.button_index = MOUSE_BUTTON_LEFT
	ev.pressed = false
	ev.position = pos
	ev.global_position = pos
	root.push_input(ev, true)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
