extends SceneTree
## Production hardening sweep for embeds (issue #56):
## - RCSS isolation: an embed's stylesheet must not leak into the parent (or vice
##   versa), even with identical selectors.
## - @media: the parent's queries AND the embed's own queries must react to
##   context resize.

const HOST_HARD := "res://tests/fixtures/embed/host_hard.rml"

var _ctx: Node
var _phase := 0
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_preset(Control.PRESET_TOP_LEFT)
	_ctx.set_size(Vector2(800, 600))
	create_timer(0.3).timeout.connect(_step)


func _w(handle) -> String:
	return str(handle.get_property("width")) if handle != null and handle.is_valid() else "<invalid>"


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document", HOST_HARD)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			# --- RCSS isolation (same selector ".swatch", different values) ---
			var pw := _w(_ctx.call("get_element_by_id", "p-swatch"))
			var ew := _w(_ctx.call("get_embedded_element", "emb", "e-swatch"))
			print("  [info] parent .swatch width=%s   embed .swatch width=%s" % [pw, ew])
			_check("parent .swatch keeps its own value (111)", pw.begins_with("111"))
			_check("embed .swatch keeps its own value (222)", ew.begins_with("222"))
			_check("RCSS does not leak across the embed boundary", pw != ew)

			# --- @media at 800px wide (min-width:600 active) ---
			var rb := _w(_ctx.call("get_element_by_id", "respbox"))
			var erb := _w(_ctx.call("get_embedded_element", "emb", "e-respbox"))
			print("  [info] @800: respbox=%s  e-respbox=%s" % [rb, erb])
			_check("parent @media active at 800 (respbox 300)", rb.begins_with("300"))
			_check("embed @media active at 800 (e-respbox 250)", erb.begins_with("250"))

			_ctx.set_size(Vector2(400, 600))
			_phase = 2
			create_timer(0.6).timeout.connect(_step)
		2:
			# --- @media after resize to 400px wide (min-width:600 inactive) ---
			var rb := _w(_ctx.call("get_element_by_id", "respbox"))
			var erb := _w(_ctx.call("get_embedded_element", "emb", "e-respbox"))
			print("  [info] @400: respbox=%s  e-respbox=%s" % [rb, erb])
			_check("parent @media reacts to resize (respbox 100)", rb.begins_with("100"))
			_check("embed @media reacts to resize (e-respbox 50)", erb.begins_with("50"))

			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
