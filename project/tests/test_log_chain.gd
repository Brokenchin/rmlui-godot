extends SceneTree
## Verifies RmlUi log forwarding: parse error -> RmlManager.get_recent_log().

var _ctx: Node


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	create_timer(0.3).timeout.connect(_run)


func _run() -> void:
	var mgr: Object = Engine.get_singleton("RmlManager") if Engine.has_singleton("RmlManager") else null
	if mgr == null:
		print("FAIL: no RmlManager singleton")
		quit(1)
		return
	print("has clear_recent_log: ", mgr.has_method("clear_recent_log"))
	print("has rml_log signal: ", mgr.has_signal("rml_log"))

	# Lambda connections to the extension singleton MUST be disconnected
	# before quit — the singleton outlives GDScript teardown and releasing a
	# closure-holding connection at extension deinit crashes the process
	# (documented on the signal; method Callables from Nodes are safe).
	var got_signal := []
	var on_log := func(level, message): got_signal.append([level, message])
	mgr.connect("rml_log", on_log)

	mgr.clear_recent_log()
	_ctx.call("inject_stylesheet", "body { color: ;;; broken")
	_ctx.call("load_document_from_string", "<rml><head></head><body><div style='width: nope;'>x</div></body>", "memory://logtest")

	var entries: Array = mgr.get_recent_log()
	print("recent_log entries: ", entries.size())
	for e in entries:
		print("  [%d] %s" % [e.get("level", -1), e.get("message", "")])
	print("signal deliveries: ", got_signal.size())
	var warnish := entries.filter(func(e): return e.get("level", 9) <= 3)
	print("PASS" if warnish.size() > 0 else "FAIL: no warning/error entries captured")
	mgr.disconnect("rml_log", on_log)
	quit(0 if warnish.size() > 0 else 1)
