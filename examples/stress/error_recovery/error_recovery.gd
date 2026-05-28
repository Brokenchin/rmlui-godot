extends Control

const FONT_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const DOC_PATH := "res://addons/rmlui-godot/examples/stress/error_recovery/error_recovery.rml"

var _pass_count: int = 0
var _fail_count: int = 0


func _ready() -> void:
	var rml: RmlContext = $RmlContext
	rml.load_font_face(FONT_PATH)

	await get_tree().process_frame
	await get_tree().process_frame
	_run_tests()


func _run_tests() -> void:
	var rml: RmlContext = $RmlContext

	# T1: load_document with nonexistent path — no crash
	rml.load_document("res://nonexistent/does_not_exist.rml")
	var docs: Array = rml.get_loaded_documents()
	_report("T1: nonexistent document not added to list", docs.size() == 0)

	# T2: load_font_face with nonexistent path — returns false
	var bad_font: bool = rml.load_font_face("res://nonexistent/bad_font.ttf")
	_report("T2: nonexistent font returns false", not bad_font)

	# T3: load_document with empty string — no crash
	rml.load_document("")
	docs = rml.get_loaded_documents()
	_report("T3: empty path document not added", docs.size() == 0)

	# T4: load_font_face with empty string — returns false
	var empty_font: bool = rml.load_font_face("")
	_report("T4: empty font path returns false", not empty_font)

	# T5: load valid document after failed loads — succeeds
	rml.load_document(DOC_PATH)
	docs = rml.get_loaded_documents()
	_report("T5: valid document loads after failures", docs.size() == 1)

	# T6: reload_document on not-loaded path — returns false
	var bad_reload: bool = rml.reload_document("res://not_loaded.rml")
	_report("T6: reload not-loaded path returns false", not bad_reload)

	# T7: unload_document with not-tracked path — returns false
	var bad_unload: bool = rml.unload_document("res://not_tracked.rml")
	_report("T7: unload not-tracked path returns false", not bad_unload)

	# T8: create_data_model with duplicate name — returns false
	var model_ok: bool = rml.create_data_model("error_test_model")
	var dup_ok: bool = rml.create_data_model("error_test_model")
	_report("T8: duplicate data model returns false", model_ok and not dup_ok)

	# T9: bind_data_variable to nonexistent model — returns false
	var bind_bad: bool = rml.bind_data_variable("nonexistent_model", "x", 42)
	_report("T9: bind to nonexistent model returns false", not bind_bad)

	# T10: get_element_by_id for nonexistent element — returns invalid handle
	var handle: RmlElementHandle = rml.get_element_by_id("element_that_doesnt_exist")
	_report("T10: nonexistent element returns invalid handle",
		handle != null and not handle.is_valid())

	# T11: operations after unloading all documents — no crash
	rml.unload_document(DOC_PATH)
	rml.set_data_variable("error_test_model", "x", 99)
	rml.dirty_all_variables("error_test_model")
	var info: Dictionary = rml.get_context_info()
	_report("T11: operations after unload — no crash", info.get("num_loaded_paths", -1) == 0)

	# T12: inject_stylesheet with no documents loaded — returns false
	var inject_ok: bool = rml.inject_stylesheet("body { color: red; }")
	_report("T12: inject_stylesheet with no docs returns false", not inject_ok)

	print("Error Recovery: %d/%d passing" % [_pass_count, _pass_count + _fail_count])
	if _fail_count > 0:
		push_warning("Error Recovery: %d tests FAILED" % _fail_count)


func _report(name: String, passed: bool) -> void:
	var status: String = "PASS" if passed else "FAIL"
	if passed:
		_pass_count += 1
	else:
		_fail_count += 1
	print("  [%s] %s" % [status, name])
