extends Control

const FONT_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const DOC_PATH := "res://addons/rmlui-godot/examples/stress/context_churn/context_churn.rml"
const WAVES := 5
const CONTEXTS_PER_WAVE := 10

var _pass_count: int = 0
var _fail_count: int = 0
var _wave: int = 0
var _ctx_serial: int = 0
var _ephemeral_contexts: Array[RmlContext] = []

@onready var sentinel: RmlContext = $Sentinel


func _ready() -> void:
	sentinel.load_font_face(FONT_PATH)
	sentinel.load_document(DOC_PATH)

	await get_tree().process_frame
	await get_tree().process_frame

	_test_sentinel_baseline()
	_run_waves()


func _test_sentinel_baseline() -> void:
	var canary: String = sentinel.get_element_outer_rml("canary")
	_report("T1: sentinel baseline — canary text present", canary.length() > 10 and "CANARY" in canary)

	var info: Dictionary = sentinel.get_context_info()
	_report("T2: sentinel baseline — context initialized", info.get("initialized", false))
	_report("T3: sentinel baseline — document loaded", info.get("num_documents", 0) > 0)


func _run_waves() -> void:
	for w in range(WAVES):
		_wave = w + 1
		await _run_single_wave()

	_test_sentinel_final()

	print("Context Churn: %d/%d passing" % [_pass_count, _pass_count + _fail_count])
	if _fail_count > 0:
		push_warning("Context Churn: %d tests FAILED" % _fail_count)


func _run_single_wave() -> void:
	var count: int = CONTEXTS_PER_WAVE
	print("  Wave %d: creating %d ephemeral contexts..." % [_wave, count])

	for i in range(count):
		_ctx_serial += 1
		var ctx := RmlContext.new()
		ctx.rml_context_name = "churn_%d" % _ctx_serial
		ctx.clip_contents = true
		ctx.size = Vector2(200, 150)
		ctx.position = Vector2(0, 0)
		add_child(ctx)
		ctx.load_font_face(FONT_PATH)
		ctx.load_document(DOC_PATH)
		_ephemeral_contexts.append(ctx)

	await get_tree().process_frame

	var all_loaded: bool = true
	for ctx in _ephemeral_contexts:
		var info: Dictionary = ctx.get_context_info()
		if not info.get("initialized", false) or info.get("num_documents", 0) == 0:
			all_loaded = false
			break
	_report("W%d: all %d ephemeral contexts loaded" % [_wave, count], all_loaded)

	var sentinel_ok_mid: bool = _check_sentinel_canary()
	_report("W%d: sentinel canary intact mid-wave" % _wave, sentinel_ok_mid)

	for ctx in _ephemeral_contexts:
		ctx.queue_free()
	_ephemeral_contexts.clear()

	await get_tree().process_frame
	await get_tree().process_frame

	var sentinel_ok_post: bool = _check_sentinel_canary()
	_report("W%d: sentinel canary intact after destroying %d contexts" % [_wave, count], sentinel_ok_post)

	var sentinel_info: Dictionary = sentinel.get_context_info()
	var sentinel_has_docs: bool = sentinel_info.get("num_documents", 0) > 0
	_report("W%d: sentinel document still loaded" % _wave, sentinel_has_docs)


func _test_sentinel_final() -> void:
	var canary: String = sentinel.get_element_outer_rml("canary")
	_report("FINAL: sentinel canary survives %d waves (%d contexts destroyed)" % [
		WAVES, WAVES * CONTEXTS_PER_WAVE],
		canary.length() > 10 and "CANARY" in canary)

	var info: Dictionary = sentinel.get_context_info()
	_report("FINAL: sentinel context still initialized", info.get("initialized", false))

	sentinel.set_element_inner_rml("status",
		"SURVIVED %d waves — %d contexts created and destroyed" % [
			WAVES, WAVES * CONTEXTS_PER_WAVE])


func _check_sentinel_canary() -> bool:
	var canary: String = sentinel.get_element_outer_rml("canary")
	return canary.length() > 10 and "CANARY" in canary


func _report(name: String, passed: bool) -> void:
	var status: String = "PASS" if passed else "FAIL"
	if passed:
		_pass_count += 1
	else:
		_fail_count += 1
	print("  [%s] %s" % [status, name])
