extends SceneTree
## Exercises the hover bridge (issue #24): rml_element_hovered /
## rml_element_unhovered signals + get_hovered_element_id(). Drives an RmlContext
## with synthetic mouse motion and asserts the enter/leave bookkeeping, including
## nearest-id-ancestor resolution for a child element that carries no id.
##
## Motion is injected through the root window viewport (which routes it into the
## SubViewportContainer with the mouse-over state Godot needs to deliver motion —
## pushing straight to a SubViewport drops lone motion events). The physical
## cursor is assumed stationary during the run, so between injected events the
## hover chain stays put and the per-frame signal bookkeeping is deterministic.
## Windowed (not headless): hover tracking runs in _process after the context
## Update, so the context must actually tick frames.

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 16px; width: 100%; height: 100%; background-color: #202020; }
#slot-a, #slot-b { display: block; width: 120px; height: 50px; background-color: #3a4254; }
#slot-b { margin-top: 20px; background-color: #445a44; }
.label { display: block; width: 100%; height: 100%; color: #ffffff; }
</style>
</head>
<body>
	<div id="slot-a"><span class="label">A</span></div>
	<div id="slot-b"><span class="label">B</span></div>
</body>
</rml>"""

var _sv: SubViewport
var _ctx: Node
var _phase := 0
var _fails := 0

var _hovered: Array = []     # [[id, pos], ...]
var _unhovered: Array = []   # [id, ...]


func _initialize() -> void:
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(400, 300)
	root.add_child(svc)
	_sv = SubViewport.new()
	_sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	svc.add_child(_sv)

	_ctx = ClassDB.instantiate(&"RmlContext")
	_sv.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_ctx.connect("rml_element_hovered", _on_hovered)
	_ctx.connect("rml_element_unhovered", _on_unhovered)
	create_timer(0.3).timeout.connect(_step)


func _on_hovered(element_id: String, global_position: Vector2) -> void:
	_hovered.append([element_id, global_position])


func _on_unhovered(element_id: String) -> void:
	_unhovered.append(element_id)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
			_ctx.call("load_document_from_string", DOC, "memory://hover_bridge")
			_advance(0.6)
		1:
			# Nothing hovered yet (cursor never moved into the context).
			_check("no events before any motion",
				_hovered.is_empty() and _unhovered.is_empty())
			_check("get_hovered_element_id() starts empty",
				_ctx.call("get_hovered_element_id") == "")
			# Enter slot-a — the point lands on the inner .label (no id), so
			# resolution must walk up to slot-a. Read synchronously: RmlUi updates
			# the hover chain inside ProcessMouseMove.
			_push_motion(Vector2(60, 25))
			_check("get_hovered_element_id() resolves id-less child to slot-a",
				_ctx.call("get_hovered_element_id") == "slot-a")
			_advance(0.3)
		2:
			_check("entering slot-a emitted one hovered(slot-a)",
				_hovered.size() == 1 and _hovered[0][0] == "slot-a")
			if _hovered.size() == 1:
				_check("hovered carries a Vector2 position",
					typeof(_hovered[0][1]) == TYPE_VECTOR2)
			_check("no unhovered yet", _unhovered.is_empty())
			# Move within slot-a — same opted-in element, must NOT re-fire.
			_push_motion(Vector2(90, 40))
			_advance(0.3)
		3:
			_check("moving within slot-a fires nothing new",
				_hovered.size() == 1 and _unhovered.is_empty())
			# Cross to slot-b: leave slot-a, enter slot-b.
			_push_motion(Vector2(60, 95))
			_check("get_hovered_element_id() now slot-b",
				_ctx.call("get_hovered_element_id") == "slot-b")
			_advance(0.3)
		4:
			_check("leaving slot-a emitted unhovered(slot-a)",
				_unhovered.size() == 1 and _unhovered[0] == "slot-a")
			_check("entering slot-b emitted hovered(slot-b)",
				_hovered.size() == 2 and _hovered[1][0] == "slot-b")
			# Move to empty body area — nothing opted-in under the cursor. The
			# context root's id (= context name) must NOT be reported.
			_push_motion(Vector2(300, 250))
			_check("get_hovered_element_id() empty over blank space (root id excluded)",
				_ctx.call("get_hovered_element_id") == "")
			_advance(0.3)
		5:
			_check("leaving slot-b over empty space emitted unhovered(slot-b)",
				_unhovered.size() == 2 and _unhovered[1] == "slot-b")
			_check("no spurious extra hovered", _hovered.size() == 2)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _advance(delay: float) -> void:
	_phase += 1
	create_timer(delay).timeout.connect(_step)


func _push_motion(pos: Vector2) -> void:
	# Inject through the root viewport (window coords == subviewport coords since
	# the container fills the window from the origin) so the container forwards it
	# with the mouse-over state Godot requires to deliver a lone motion event.
	var ev := InputEventMouseMotion.new()
	ev.position = pos
	ev.global_position = pos
	root.push_input(ev, true)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
