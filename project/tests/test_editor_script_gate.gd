extends SceneTree
## Issue #29: inline <script>/gdscript: blocks must not run in the editor unless
## the preview opts in. The gate is `is_editor_hint() && !editor_scripts_enabled`,
## checked in GodotScriptDocument::_ensure_instance and the gdscript: listener.
##
## is_editor_hint() is false under `-s`, so this run exercises the RUNTIME side:
## the flag's API surface (exists, defaults off, round-trips) and the invariant
## that runtime dispatch is NEVER gated — instances are created even with the
## flag off. The editor-true branch (scripts withheld) is verified by launching
## the editor preview, since is_editor_hint() can't be forced from a SceneTree.

const DOC := """<rml>
<head>
<script>
var rml_context
var made := true
func _ping(_e):
	if rml_context: rml_context.set_meta("pinged", true)
</script>
</head>
<body><span id="s">hi</span></body>
</rml>"""

var _ctx: Node
var _fails := 0


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	# --- API surface ---
	_check("has set_editor_scripts_enabled", _ctx.has_method("set_editor_scripts_enabled"))
	_check("has is_editor_scripts_enabled", _ctx.has_method("is_editor_scripts_enabled"))
	_check("defaults to false", _ctx.call("is_editor_scripts_enabled") == false)
	_ctx.call("set_editor_scripts_enabled", true)
	_check("round-trips true", _ctx.call("is_editor_scripts_enabled") == true)
	_ctx.call("set_editor_scripts_enabled", false)
	_check("round-trips false", _ctx.call("is_editor_scripts_enabled") == false)

	# --- Runtime invariant: gate flag OFF, yet scripts still instance/run ---
	# Wait one beat so the context's deferred _ready() creates the RmlUi context
	# before loading (load_document_from_string is a no-op before that).
	create_timer(0.4).timeout.connect(_do_load)


func _do_load() -> void:
	_ctx.call("load_document_from_string", DOC, "memory://gate")
	create_timer(0.5).timeout.connect(_after_load)


func _after_load() -> void:
	# get_document_scripts() forces _ensure_instance. Outside the editor the gate
	# is inert, so the block must instantiate despite editor_scripts_enabled=false.
	var scripts: Array = _ctx.call("get_document_scripts", "memory://gate")
	_check("runtime instances despite flag off", scripts.size() == 1)
	if scripts.size() == 1 and scripts[0] != null:
		_check("instance is the block class", scripts[0].get("made") == true)

	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
