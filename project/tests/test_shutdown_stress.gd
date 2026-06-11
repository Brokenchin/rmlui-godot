extends SceneTree
## Shutdown-ordering stress: build the messiest live state we can — multiple
## contexts, documents, script blocks, data models, injected styles, element
## handles still referenced — then quit IMMEDIATELY without any cleanup.
## Teardown must survive in any destruction order. Run repeatedly by
## run_all.sh (the reported crash is occasional, low-address ~0x10 — a
## vtable/member call through a freed pointer).

const DOC := """<rml>
<head>
<style>body { font-family: "Noto Sans"; font-size: 16px; } div { display: block; }</style>
<script>
var rml_context
func _on_load(_e):
	pass
</script>
</head>
<body onload="gdscript:_on_load" data-model="m%d">
	<div id="target">stress</div>
	<div data-for="item : items">{{ item }}</div>
</body>
</rml>"""

var _handles := []  # element handles deliberately kept alive across quit
var _sv: SubViewport


func _initialize() -> void:
	create_timer(0.2).timeout.connect(_build_and_quit)
	# Half the contexts live under a SubViewport (the preview-panel shape).
	var svc := SubViewportContainer.new()
	svc.stretch = true
	svc.size = Vector2(300, 200)
	root.add_child(svc)
	_sv = SubViewport.new()
	svc.add_child(_sv)


func _build_and_quit() -> void:
	var sv := _sv
	for i in range(6):
		var ctx = ClassDB.instantiate(&"RmlContext")
		ctx.name = "Ctx%d" % i
		if i % 2 == 0:
			root.add_child(ctx)
		else:
			sv.add_child(ctx)
		ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
		ctx.call("load_document_from_string", DOC % i, "memory://stress_%d" % i)
		ctx.call("create_data_model", "m%d" % i)
		ctx.call("bind_data_variable", "m%d" % i, "value", i)
		ctx.call("bind_data_array", "m%d" % i, "items", ["a", "b", "c"])
		ctx.call("inject_stylesheet", "#target { color: #ff00%02x; }" % (i * 40))
		# Force script-block instantiation + keep an element handle alive.
		ctx.call("get_document_script", "memory://stress_%d" % i)
		var handle = ctx.call("get_element_by_id", "target")
		if handle != null:
			_handles.append(handle)

	# One context that gets freed RIGHT at quit time (queue_free pending).
	var doomed = ClassDB.instantiate(&"RmlContext")
	root.add_child(doomed)
	doomed.call("load_document_from_string", "<rml><head></head><body>doomed</body>", "memory://doomed")
	doomed.queue_free()

	print("SHUTDOWN_STRESS_READY")
	quit(0)
