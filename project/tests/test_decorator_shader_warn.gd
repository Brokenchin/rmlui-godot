extends SceneTree
## Issue #29: a missing decorator shader must be reported ONCE per shader name,
## not once per decorated element. RmlUi calls CompileShader per element on every
## (re)load; the editor reloads on every keystroke, so a per-element push_warning
## storm froze live editing. The fix dedupes per name and routes the notice
## through notify_log (cheap), reserving the costly console warning for runtime.
##
## This run (is_editor_hint()==false) verifies the dedup: a grid of N slots all
## using one unregistered shader yields exactly ONE "No decorator shader" entry
## in RmlManager.get_recent_log(), regardless of N.

const SLOTS := 16
const SHADER := "rarity_fx_test"

var _ctx: Node
var _mgr: Object
var _fails := 0


func _doc() -> String:
	var sb := "<rml><head><style>\n"
	sb += "body{font-family:\"Noto Sans\";}\n"
	sb += ".slot{display:inline-block;width:20px;height:20px;decorator:shader(\"%s\");}\n" % SHADER
	sb += "</style></head><body>\n"
	for i in SLOTS:
		sb += "<div class=\"slot\">%d</div>" % i
	sb += "</body></rml>"
	return sb


func _initialize() -> void:
	_mgr = Engine.get_singleton("RmlManager") if Engine.has_singleton("RmlManager") else null
	_check("RmlManager present", _mgr != null)
	if _mgr == null:
		quit(1)
		return
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(300, 300)
	root.add_child(svc)
	var sv := SubViewport.new()
	sv.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	svc.add_child(sv)
	_ctx = ClassDB.instantiate(&"RmlContext")
	sv.add_child(_ctx)
	_ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	# Wait for the deferred _ready/_create_context before loading.
	create_timer(0.4).timeout.connect(_load)


func _load() -> void:
	if _mgr.has_method("clear_recent_log"):
		_mgr.call("clear_recent_log")
	_ctx.call("load_document_from_string", _doc(), "memory://deco_warn")
	# Let layout + decorator-data generation run.
	create_timer(0.6).timeout.connect(_check_log)


func _check_log() -> void:
	var needle := "No decorator shader registered for: " + SHADER
	var count := 0
	for entry in _mgr.call("get_recent_log"):
		if String(entry.get("message", "")).contains(needle):
			count += 1
	print("  '%s' entries in recent log: %d (slots=%d)" % [needle, count, SLOTS])
	# Exactly one despite SLOTS elements sharing the decorator.
	_check("missing-shader warning deduped to once per name", count == 1)
	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
