extends SceneTree
## Keyboard/gamepad focus navigation into embeds (issue #56). With gamepad
## navigation on, tabbing must be able to focus a focusable element INSIDE an
## embed, and ui_accept must activate it (firing the embed's own handler).
## The embed's button (w-self-btn) is the only tabbable element, so one TAB
## focuses it.

const HOST := "res://tests/fixtures/embed/host.rml"

var _ctx: Node
var _phase := 0
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_ctx.set_size(Vector2(800, 600))
	_ctx.set_gamepad_navigation(true)
	create_timer(0.3).timeout.connect(_step)


func _key(keycode: int, pressed: bool) -> void:
	var e := InputEventKey.new()
	e.keycode = keycode
	e.physical_keycode = keycode
	e.pressed = pressed
	_ctx.get_viewport().push_input(e, true)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document", HOST)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			# Tab (ui_focus_next defaults to TAB) — focus the embed's button.
			_key(KEY_TAB, true)
			_key(KEY_TAB, false)
			_phase = 2
			create_timer(0.3).timeout.connect(_step)
		2:
			var focused = _ctx.call("get_focused_element")
			var fid := str(focused.get_id()) if focused != null and focused.is_valid() else "<none>"
			print("  [info] focused id=%s" % fid)
			_check("tab focuses an element inside the embed", fid == "w-self-btn")

			# Accept (ui_accept defaults to ENTER) — activate the focused embed button.
			_key(KEY_ENTER, true)
			_key(KEY_ENTER, false)
			_phase = 3
			create_timer(0.3).timeout.connect(_step)
		3:
			_check("ui_accept activated the focused embed element",
				int(_ctx.get_meta("widget_self_clicked", 0)) >= 1)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
