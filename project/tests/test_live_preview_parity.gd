extends SceneTree
## Regression test for the live-edit cascade bug: rendering hello_world via
## the live-RCSS splice path with UNCHANGED content must produce (nearly) the
## same pixels as a plain disk load. The old inject-on-top path flipped the
## cascade (live rcss overrode the document's <style> block) and failed this.

const PanelScript := preload("res://addons/rmlui-godot/editor/rml_preview_panel.gd")
const DOC := "res://addons/rmlui-godot/examples/basic/hello_world/hello.rml"
const RCSS := "res://addons/rmlui-godot/examples/basic/hello_world/hello.rcss"

var _sv: SubViewport
var _ctx: Node
var _img_disk: Image
var _phase := 0
var _fails := 0


func _initialize() -> void:
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(640, 360)
	root.add_child(svc)
	_sv = SubViewport.new()
	_sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	svc.add_child(_sv)

	_spawn()
	create_timer(0.2).timeout.connect(func():
		_load_fonts()
		_ctx.call("load_document", DOC)
		create_timer(1.0).timeout.connect(_step))


func _spawn() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	_sv.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)


func _load_fonts() -> void:
	# Deferred: fonts can only load after the context's _ready initialized RmlUi.
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")


func _step() -> void:
	var info: Dictionary = _ctx.call("get_context_info")
	print("phase %d: ctx.size=%s sv.size=%s rml_dims=%dx%d" % [
		_phase, _ctx.size, _sv.size, info.get("width", -1), info.get("height", -1)])
	if _phase == 0:
		_img_disk = _sv.get_texture().get_image()
		_img_disk.save_png("res://tests/_parity_disk.png")
		_phase = 1
		# Phase 1: splice path (live .rcss editing) with content identical to disk.
		var rml_text := FileAccess.get_file_as_string(DOC)
		var rcss_text := FileAccess.get_file_as_string(RCSS)
		var spliced: String = PanelScript._splice_live_rcss(rml_text, "hello.rcss", rcss_text)
		_reload_with(spliced, "splice")
	elif _phase == 1:
		_fails += _compare("splice (live rcss)")
		_phase = 2
		# Phase 2: raw from-string path (live .rml editing) — buffer content
		# identical to disk, so the render must match the disk load.
		_reload_with(FileAccess.get_file_as_string(DOC), "raw")
	else:
		_fails += _compare("raw from-string (live rml)")
		print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
		quit(_fails)


func _reload_with(text: String, tag: String) -> void:
	if text.is_empty():
		print("FAIL: empty document text for ", tag)
		quit(1)
		return
	_ctx.queue_free()
	_spawn()
	create_timer(0.2).timeout.connect(func():
		_load_fonts()
		_ctx.call("load_document_from_string", text, DOC)
		create_timer(1.0).timeout.connect(_step))


func _compare(tag: String) -> int:
	var img_live := _sv.get_texture().get_image()
	var diff := 0
	var total := 0
	for y in range(0, img_live.get_height(), 2):
		for x in range(0, img_live.get_width(), 2):
			total += 1
			var a := _img_disk.get_pixel(x, y)
			var b := img_live.get_pixel(x, y)
			if abs(a.r - b.r) + abs(a.g - b.g) + abs(a.b - b.b) > 0.1:
				diff += 1
	var pct := 100.0 * diff / total
	img_live.save_png("res://tests/_parity_%s.png" % tag.get_slice(" ", 0))
	print("%s: pixel diff %d/%d (%.2f%%) -> %s" % [tag, diff, total, pct, "PASS" if pct < 2.0 else "FAIL"])
	return 0 if pct < 2.0 else 1
