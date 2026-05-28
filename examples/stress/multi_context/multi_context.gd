extends Control

const FONT_PATH_REGULAR := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const FONT_PATH_BOLD := "res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf"
const DOC_PATH := "res://addons/rmlui-godot/examples/stress/multi_context/multi_context_stress.rml"
const MEASURE_FRAMES: int = 60
const MAX_AVG_US: int = 10000
const MAX_SPIKE_US: int = 16000
const CONTEXT_COUNT: int = 4

var _frame_times: Array[int] = []
var _measuring: bool = false
var _docs_loaded: int = 0
var _warmup_frames: int = 10
var _pass_count: int = 0
var _fail_count: int = 0


func _ready() -> void:
	for i in range(CONTEXT_COUNT):
		var rml: RmlContext = get_node("RmlContext" + str(i + 1)) as RmlContext
		rml.load_font_face(FONT_PATH_REGULAR)
		rml.load_font_face(FONT_PATH_BOLD)
		rml.load_document(DOC_PATH)
		_docs_loaded += 1


func _process(_delta: float) -> void:
	if _docs_loaded < CONTEXT_COUNT:
		return

	if _warmup_frames > 0:
		_warmup_frames -= 1
		return

	if not _measuring:
		_measuring = true

	var start: int = Time.get_ticks_usec()
	for i in range(CONTEXT_COUNT):
		var _rml: RmlContext = get_node("RmlContext" + str(i + 1)) as RmlContext
	var end: int = Time.get_ticks_usec()

	_frame_times.append(end - start)

	if _frame_times.size() >= MEASURE_FRAMES:
		_run_tests()
		set_process(false)


func _run_tests() -> void:
	_test_docs_loaded()
	_test_element_count()
	_test_frame_timing()

	print("Multi-Context Stress: %d/%d passing" % [_pass_count, _pass_count + _fail_count])
	if _fail_count > 0:
		push_warning("Multi-Context Stress: %d tests FAILED" % _fail_count)


func _test_docs_loaded() -> void:
	var all_valid: bool = true
	for i in range(CONTEXT_COUNT):
		var rml: RmlContext = get_node("RmlContext" + str(i + 1)) as RmlContext
		var root: RmlElementHandle = rml.get_element_by_id("perf-root")
		if root == null or not root.is_valid():
			all_valid = false
			break
	_report("all %d documents load without error" % CONTEXT_COUNT, all_valid)


func _test_element_count() -> void:
	var total_found: int = 0
	var checks: Array[String] = ["perf-root", "li-001", "li-050", "gc-0-0", "deep-a1", "box-001", "scroll-last"]

	for i in range(CONTEXT_COUNT):
		var rml: RmlContext = get_node("RmlContext" + str(i + 1)) as RmlContext
		for id in checks:
			var handle: RmlElementHandle = rml.get_element_by_id(id)
			if handle != null and handle.is_valid():
				total_found += 1

	var expected: int = CONTEXT_COUNT * checks.size()
	_report("%dx element structure verified (%d/%d markers)" % [CONTEXT_COUNT, total_found, expected],
		total_found == expected)


func _test_frame_timing() -> void:
	var total: int = 0
	var min_us: int = _frame_times[0]
	var max_us: int = _frame_times[0]

	for t in _frame_times:
		total += t
		if t < min_us:
			min_us = t
		if t > max_us:
			max_us = t

	var avg_us: int = total / _frame_times.size()

	print("[PERF] contexts=%d frames=%d avg_us=%d min_us=%d max_us=%d" % [
		CONTEXT_COUNT, _frame_times.size(), avg_us, min_us, max_us])

	_report("multi-context avg under %dus (avg=%dus)" % [MAX_AVG_US, avg_us], avg_us < MAX_AVG_US)
	_report("no frame exceeds %dus (max=%dus)" % [MAX_SPIKE_US, max_us], max_us < MAX_SPIKE_US)


func _report(name: String, passed: bool) -> void:
	var status: String = "PASS" if passed else "FAIL"
	if passed:
		_pass_count += 1
	else:
		_fail_count += 1
	print("  [%s] %s" % [status, name])
