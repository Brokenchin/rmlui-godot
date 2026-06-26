extends SceneTree
## Regression test for issue #25: <script src="…"> external GDScript loading.
##
## RmlUi stores external resource paths with ':' URL-encoded as '|', so the
## document's "res://" scheme reached LoadExternalScript() as "res|//…" and the
## Godot ResourceLoader failed with "Can't open file". RCSS <link href> was
## unaffected because it decodes via StreamFile. This exercises real on-disk
## documents (the bug only manifests with file-based src resolution, not the
## memory:// strings used by test_inline_gdscript.gd):
##   - relative src resolved against the document's directory
##   - absolute res:// src passed through
##   - the loaded script's _on_load handler dispatches and rml_context injects

const DOC_RELATIVE := "res://tests/fixtures/external_script/view_frame_doc.rml"
const DOC_ABSOLUTE := "res://tests/fixtures/external_script/view_frame_doc_abs.rml"

var _ctx: Node
var _phase := 0
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	create_timer(0.3).timeout.connect(_step)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document", DOC_RELATIVE)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			_check("relative src: handler ran", int(_ctx.get_meta("external_script_ran", 0)) == 1)
			_check("relative src: rml_context injected", _ctx.has_meta("external_script_ran"))
			_check("relative src: event passed", str(_ctx.get_meta("external_event_type", "")) == "load")
			# The loaded script instance must be reachable + callable.
			var inst = _ctx.call("get_document_script", DOC_RELATIVE)
			_check("relative src: instance reachable", inst != null)
			if inst != null:
				_check("relative src: method callable", str(inst.call("render")) == "external-ok")
			_ctx.call("unload_document", DOC_RELATIVE)
			_ctx.remove_meta("external_script_ran")
			_ctx.call("load_document", DOC_ABSOLUTE)
			_phase = 2
			create_timer(0.5).timeout.connect(_step)
		2:
			_check("absolute src: handler ran", int(_ctx.get_meta("external_script_ran", 0)) == 1)
			_check("absolute src: rml_context injected", _ctx.has_meta("external_script_ran"))
			_ctx.call("unload_document", DOC_ABSOLUTE)
			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
