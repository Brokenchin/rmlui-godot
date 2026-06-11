extends SceneTree
## Verifies RmlManager.get_supported_rcss_properties() returns the engine list.

var _ctx: Node


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")  # initializes RmlUi
	root.add_child(_ctx)
	create_timer(0.3).timeout.connect(_run)


func _run() -> void:
	var mgr: Object = Engine.get_singleton("RmlManager")
	var props: PackedStringArray = mgr.get_supported_rcss_properties()
	print("property count: ", props.size())
	var expected := ["width", "color", "display", "decorator", "drag", "margin", "padding", "flex", "nav-up"]
	var missing := []
	for e in expected:
		if not e in props:
			missing.append(e)
	print("sample: ", ", ".join(PackedStringArray(Array(props).slice(0, 12))))
	if props.size() > 80 and missing.is_empty():
		print("PASS")
		quit(0)
	else:
		print("FAIL — missing: ", missing)
		quit(1)
