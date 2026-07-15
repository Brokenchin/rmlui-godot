extends SceneTree
## Issue #36: editing an inline <script> with a mid-edit GDScript syntax error
## (e.g. an unclosed array literal) froze the editor. Cause: compiling the block
## calls GDScript::reload(), which on a parse error breaks into the editor's
## debugger. The diagnostics validator reloads the document on every keystroke,
## so the freeze hit by default — with scripts NEVER opted in.
##
## Fix: GodotScriptDocument::LoadInlineScript skips compilation entirely when
## inline scripts are gated in the editor (is_editor_hint() && !opted_in),
## mirroring the execution gate from #29.
##
## is_editor_hint() is false under `-s`, so this run exercises the RUNTIME side:
## the broken-array document must load WITHOUT crashing the process and produce
## zero compiled blocks (graceful compile failure), while a sibling valid block
## still compiles. The editor-true branch (compile withheld -> no debugger break)
## is verified by launching the editor preview, since is_editor_hint() can't be
## forced from a SceneTree — same limitation as test_editor_script_gate.gd.

# The exact repro shape: an inline block whose array literal is never closed.
const DOC_BROKEN := """<rml>
<head>
<script>
var rml_context
var items := [1, 2, 3
func _on_load(_e):
	pass
</script>
</head>
<body onload="gdscript:_on_load"><div>broken</div></body>
</rml>"""

const DOC_VALID := """<rml>
<head>
<script>
var rml_context
var made := true
func _on_load(_e):
	pass
</script>
</head>
<body onload="gdscript:_on_load"><div>ok</div></body>
</rml>"""

var _ctx: Node
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	# Wait one beat so the deferred _ready() creates the RmlUi context before
	# loading (load_document_from_string is a no-op before that).
	create_timer(0.4).timeout.connect(_step_broken)


func _step_broken() -> void:
	# The line that froze the editor: loading a document whose inline script has
	# a syntax error. At runtime reload() returns an error without breaking; the
	# process must stay alive and the broken block must not produce an instance.
	_ctx.call("load_document_from_string", DOC_BROKEN, "memory://broken")
	create_timer(0.4).timeout.connect(_check_broken)


func _check_broken() -> void:
	_check("survives loading a malformed inline script", is_instance_valid(_ctx))
	var scripts: Array = _ctx.call("get_document_scripts", "memory://broken")
	_check("malformed block produced no instance", scripts.size() == 0)
	_ctx.call("unload_document", "memory://broken")
	_ctx.call("load_document_from_string", DOC_VALID, "memory://valid")
	create_timer(0.4).timeout.connect(_check_valid)


func _check_valid() -> void:
	# A well-formed sibling block still compiles and runs at runtime (the gate is
	# inert outside the editor), proving the fix only withholds the broken case.
	var scripts: Array = _ctx.call("get_document_scripts", "memory://valid")
	_check("valid block still compiles at runtime", scripts.size() == 1)
	if scripts.size() == 1 and scripts[0] != null:
		_check("valid instance is the block class", scripts[0].get("made") == true)

	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
