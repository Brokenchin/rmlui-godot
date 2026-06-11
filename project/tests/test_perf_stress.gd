extends SceneTree
## Master-plan stress target: 8 contexts, 1000+ elements, rapid data updates,
## sustained 60 fps. Every context re-renders every frame (value churn dirties
## the model; one array mutation per frame forces layout work too).
## Windowed — rendering must actually happen.

const CONTEXTS := 8
const ITEMS_PER_CONTEXT := 125  # 8 * 125 = 1000 data-driven elements minimum
const WARMUP_FRAMES := 30
const MEASURED_FRAMES := 180
const BUDGET_AVG_MS := 16.0
const BUDGET_WORST_MS := 50.0  # allow isolated scheduler spikes, catch hitching

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 11px; color: #dddddd; width: 100%%; height: 100%%; background-color: #1a1a2e; }
.item { display: block; margin: 1px; padding: 1px; background-color: #16213e; }
.item:hover { background-color: #0f3460; }
#counter { color: #00ff88; }
</style>
</head>
<body data-model="stress%d">
	<div id="counter">{{ counter }}</div>
	<div class="item" data-for="item : items">{{ item }} · {{ counter }}</div>
</body>
</rml>"""

var _contexts := []
var _frame := 0
var _measuring := false
var _frame_times: PackedFloat64Array = []
var _last_usec := 0


func _initialize() -> void:
	# Vsync pins every frame to 16.67 ms — disable to measure real throughput.
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	root.size = Vector2i(1280, 720)
	for i in range(CONTEXTS):
		var ctx = ClassDB.instantiate(&"RmlContext")
		root.add_child(ctx)
		ctx.position = Vector2((i % 4) * 320, (i / 4) * 360)
		ctx.size = Vector2(320, 360)
		_contexts.append(ctx)
	create_timer(0.3).timeout.connect(_setup)


func _setup() -> void:
	for i in range(CONTEXTS):
		var ctx = _contexts[i]
		ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
		var model := "stress%d" % i
		ctx.call("create_data_model", model)
		ctx.call("bind_data_variable", model, "counter", 0)
		var items := []
		for j in range(ITEMS_PER_CONTEXT):
			items.append("item %d/%d" % [i, j])
		ctx.call("bind_data_array", model, "items", items)
		ctx.call("load_document_from_string", DOC % i, "memory://stress%d" % i)

	_last_usec = Time.get_ticks_usec()
	root.get_tree().process_frame.connect(_on_frame)


func _on_frame() -> void:
	var now := Time.get_ticks_usec()
	var dt_ms := (now - _last_usec) / 1000.0
	_last_usec = now

	_frame += 1
	if _frame == WARMUP_FRAMES:
		_measuring = true
	elif _measuring:
		_frame_times.append(dt_ms)

	# Rapid updates: every context, every frame — counter churn + one array
	# mutation (forces element re-creation in the data-for view).
	for i in range(CONTEXTS):
		var model := "stress%d" % i
		_contexts[i].call("set_data_variable", model, "counter", _frame)
		_contexts[i].call("set_data_array_item", model, "items", _frame % ITEMS_PER_CONTEXT,
			"item %d updated @%d" % [i, _frame])

	if _frame >= WARMUP_FRAMES + MEASURED_FRAMES:
		_finish()


func _finish() -> void:
	root.get_tree().process_frame.disconnect(_on_frame)
	var total := 0.0
	var worst := 0.0
	for t in _frame_times:
		total += t
		worst = maxf(worst, t)
	var avg := total / _frame_times.size()
	var fps := 1000.0 / avg
	print("contexts=%d elements>=%d frames=%d" % [CONTEXTS, CONTEXTS * ITEMS_PER_CONTEXT, _frame_times.size()])
	print("avg frame: %.2f ms (%.0f fps) · worst: %.2f ms" % [avg, fps, worst])
	var ok := avg < BUDGET_AVG_MS and worst < BUDGET_WORST_MS
	print("PASS" if ok else "FAIL (budget: avg<%.0fms worst<%.0fms)" % [BUDGET_AVG_MS, BUDGET_WORST_MS])
	quit(0 if ok else 1)
