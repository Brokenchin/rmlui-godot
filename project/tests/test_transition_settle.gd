extends SceneTree
## Issue #55 — a CSS transition's FINAL frame must be painted. On the frame a
## transition completes, Update() applies the final values AND
## GetNextUpdateDelay() flips away from 0 in the same tick, so the old redraw
## gate never queued the settled frame: the element stuck one animation step
## short until an unrelated dirty (mouse hover) forced a repaint.
##
## Method (no input events at all — nothing touches the mouse): trigger the
## transition, wait for it to settle, snapshot. Then force a known-good repaint
## with a visually inert mutation (toggling a class with no style rules marks
## the frame render-dirty) and snapshot again. If the settled frame was painted,
## the two snapshots are pixel-identical; if the bug is present the first one
## shows the second-to-last animation step and they differ.
##
## Covers a top-level document (#panel) and an <embed-doc> sub-document
## (#epanel), sequentially. Windowed (needs real pixels) — see tests/run_all.sh.

const HOST := "res://tests/fixtures/transition/settle_host.rml"

var _sv: SubViewport
var _ctx: Node
var _phase := 0
var _fails := 0
var _before: Image
var _settled: Image


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
	create_timer(0.3).timeout.connect(_setup)


func _setup() -> void:
	_ctx.call("load_document", HOST)
	create_timer(0.6).timeout.connect(_step)


func _snap() -> Image:
	return _sv.get_texture().get_image()


func _diff(a: Image, b: Image) -> int:
	var n := 0
	for y in range(0, b.get_height(), 2):
		for x in range(0, b.get_width(), 2):
			var pa := a.get_pixel(x, y)
			var pb := b.get_pixel(x, y)
			if absf(pa.r - pb.r) + absf(pa.g - pb.g) + absf(pa.b - pb.b) > 0.1:
				n += 1
	return n


func _step() -> void:
	match _phase:
		# ---- top-level document ----
		0:
			_before = _snap()
			_ctx.call("set_element_class", "panel", "shifted", true)
			_phase = 1
			create_timer(0.15).timeout.connect(_step)
		1:
			_check("top-level transition is animating", _diff(_before, _snap()) > 0)
			_phase = 2
			create_timer(1.0).timeout.connect(_step)  # 0.4s transition, generous margin
		2:
			_settled = _snap()
			_check("top-level element moved from its start", _diff(_before, _settled) > 0)
			# Visually inert mutation → marks render dirty → known-good repaint.
			_ctx.call("set_element_class", "panel", "nop-no-such-rule", true)
			_phase = 3
			create_timer(0.4).timeout.connect(_step)
		3:
			var d := _diff(_settled, _snap())
			_check("top-level settled frame was already painted (diff=%d)" % d, d == 0)
			_phase = 4
			_step()
		# ---- embedded (<embed-doc>) document ----
		4:
			_before = _snap()
			var h = _ctx.call("get_embedded_element", "emb", "epanel")
			var ok: bool = h != null and h.is_valid()
			_check("embedded element resolved", ok)
			if not ok:
				_finish()
				return
			h.set_class("shifted", true)
			_phase = 5
			create_timer(0.15).timeout.connect(_step)
		5:
			_check("embedded transition is animating", _diff(_before, _snap()) > 0)
			_phase = 6
			create_timer(1.0).timeout.connect(_step)
		6:
			_settled = _snap()
			_check("embedded element moved from its start", _diff(_before, _settled) > 0)
			var h = _ctx.call("get_embedded_element", "emb", "epanel")
			h.set_class("nop-no-such-rule", true)
			_phase = 7
			create_timer(0.4).timeout.connect(_step)
		7:
			var d := _diff(_settled, _snap())
			_check("embedded settled frame was already painted (diff=%d)" % d, d == 0)
			_finish()


func _finish() -> void:
	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
