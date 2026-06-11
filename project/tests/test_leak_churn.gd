extends SceneTree
## Leak meter: churns full context lifecycles (create → fonts → document with
## script block → data model → element handles → destroy) and samples static
## memory + object count. After warmup, growth across measured cycles must be
## bounded — unbounded growth = a leak in the teardown paths.

const CYCLES_WARMUP := 10
const CYCLES_MEASURED := 30
const DOC := """<rml>
<head>
<style>body { font-family: "Noto Sans"; font-size: 14px; }</style>
<script>
var rml_context
func _on_load(_e):
	rml_context.set_data_variable("m", "value", 42)
</script>
</head>
<body onload="gdscript:_on_load" data-model="m">
	<div id="el">churn</div>
	<div data-for="item : items">{{ item }}</div>
</body>
</rml>"""

var _cycle := 0
var _mem_after_warmup := 0
var _obj_after_warmup := 0


func _initialize() -> void:
	create_timer(0.2).timeout.connect(_run_cycle)


func _run_cycle() -> void:
	var ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(ctx)
	ctx.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	ctx.call("load_document_from_string", DOC, "memory://churn")
	ctx.call("create_data_model", "m")
	ctx.call("bind_data_variable", "m", "value", 0)
	ctx.call("bind_data_array", "m", "items", ["x", "y", "z"])
	ctx.call("get_document_script", "memory://churn")
	var handle = ctx.call("get_element_by_id", "el")  # dropped each cycle
	if handle == null:
		pass
	ctx.call("reload_document", "memory://churn")  # hot-reload path each cycle
	root.remove_child(ctx)
	ctx.free()

	_cycle += 1
	if _cycle == CYCLES_WARMUP:
		_mem_after_warmup = OS.get_static_memory_usage()
		_obj_after_warmup = Performance.get_monitor(Performance.OBJECT_COUNT)
	if _cycle < CYCLES_WARMUP + CYCLES_MEASURED:
		# A breather frame lets queued deletions actually run.
		create_timer(0.03).timeout.connect(_run_cycle)
		return

	var mem_growth := OS.get_static_memory_usage() - _mem_after_warmup
	var obj_growth: int = int(Performance.get_monitor(Performance.OBJECT_COUNT)) - _obj_after_warmup
	var per_cycle := float(mem_growth) / CYCLES_MEASURED
	print("memory growth over %d cycles: %d bytes (%.1f/cycle)" % [CYCLES_MEASURED, mem_growth, per_cycle])
	print("object count growth: %d" % obj_growth)

	# Thresholds: a real leak (per-cycle texture/mesh/script retention) shows
	# as tens of KB per cycle; allocator noise stays far below.
	var mem_ok := per_cycle < 4096.0
	var obj_ok := obj_growth <= 5  # near-zero; engine-side noise only
	print("PASS" if (mem_ok and obj_ok) else "FAIL (mem_ok=%s obj_ok=%s)" % [mem_ok, obj_ok])
	quit(0 if (mem_ok and obj_ok) else 1)
