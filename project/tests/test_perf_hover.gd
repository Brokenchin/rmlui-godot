extends SceneTree
## Representative real-world stress: a single large grid that the user hovers
## across. Each frame the cursor moves one cell, so RmlUi re-styles exactly the
## cell being entered and the one being left (:hover background swap) — only a
## couple of elements change geometry, the other ~1000 are identical frame to
## frame. This is the case issue #14 calls out ("very expensive to hover a mouse
## over a large grid"): the OLD pipeline tore down and rebuilt the entire
## canvas-item tree every frame regardless. Windowed — rendering must happen.

const COLS := 40
const ROWS := 26          # 40 * 26 = 1040 cells
const CELL_W := 30
const CELL_H := 20
const WARMUP_FRAMES := 30
const MEASURED_FRAMES := 180

# Debug-DLL budgets (suite runs the debug build; ~3x slower, no optimizer).
const BUDGET_AVG_MS_DEBUG := 24.0
const BUDGET_WORST_MS_DEBUG := 90.0
const BUDGET_AVG_MS_RELEASE := 8.0
const BUDGET_WORST_MS_RELEASE := 30.0

const DOC := """<rml>
<head>
<style>
body { font-family: "Noto Sans"; font-size: 10px; color: #cccccc; width: 100%%; height: 100%%; background-color: #12121a; }
.row { display: block; }
.cell { display: inline-block; width: 28px; height: 18px; margin: 0px; padding: 0px; background-color: #1e2a44; }
.cell:hover { background-color: #ff8800; }
</style>
</head>
<body>
%s
</body>
</rml>"""

var _ctx: Node
var _frame := 0
var _measuring := false
var _frame_times: PackedFloat64Array = []
var _last_usec := 0
var _vp: Viewport
var _origin := Vector2.ZERO


func _initialize() -> void:
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	root.size = Vector2i(COLS * CELL_W + 40, ROWS * CELL_H + 40)
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.position = Vector2(20, 20)
	_ctx.size = Vector2(COLS * CELL_W, ROWS * CELL_H)
	_origin = Vector2(20, 20)
	_vp = root
	create_timer(0.3).timeout.connect(_setup)


func _setup() -> void:
	_ctx.call("load_font_face", "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	var rml := ""
	for r in range(ROWS):
		rml += "\t<div class=\"row\">"
		for c in range(COLS):
			rml += "<div class=\"cell\">%d</div>" % (r * COLS + c)
		rml += "</div>\n"
	_ctx.call("load_document_from_string", DOC % rml, "memory://hovergrid")
	create_timer(0.6).timeout.connect(_start)


func _start() -> void:
	_last_usec = Time.get_ticks_usec()
	root.get_tree().process_frame.connect(_on_frame)


func _hover_at(cell_index: int) -> void:
	# Centre of the target cell, in window coordinates.
	var col := cell_index % COLS
	var row := (cell_index / COLS) % ROWS
	var pos := _origin + Vector2(col * CELL_W + CELL_W * 0.5, row * CELL_H + CELL_H * 0.5)
	var ev := InputEventMouseMotion.new()
	ev.position = pos
	ev.global_position = pos
	_vp.push_input(ev)


func _on_frame() -> void:
	var now := Time.get_ticks_usec()
	var dt_ms := (now - _last_usec) / 1000.0
	_last_usec = now

	_frame += 1
	if _frame == WARMUP_FRAMES:
		_measuring = true
	elif _measuring:
		_frame_times.append(dt_ms)

	# Sweep the cursor one cell per frame: enters one cell, leaves the previous.
	_hover_at(_frame)

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
	print("hover grid: cells=%d frames=%d" % [COLS * ROWS, _frame_times.size()])
	print("avg frame: %.2f ms (%.0f fps) · worst: %.2f ms" % [avg, fps, worst])
	var budget_avg := BUDGET_AVG_MS_DEBUG if OS.is_debug_build() else BUDGET_AVG_MS_RELEASE
	var budget_worst := BUDGET_WORST_MS_DEBUG if OS.is_debug_build() else BUDGET_WORST_MS_RELEASE
	var ok := avg < budget_avg and worst < budget_worst
	print("PASS" if ok else "FAIL (budget: avg<%.0fms worst<%.0fms)" % [budget_avg, budget_worst])
	quit(0 if ok else 1)
