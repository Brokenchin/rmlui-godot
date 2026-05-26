extends SceneTree

var _passed := 0
var _failed := 0
var _errors: Array[String] = []

func _init():
	print("\n========================================")
	print("  RmlUI-Godot Hardening Test Suite")
	print("========================================\n")

func _initialize():
	var tests: Array[Script] = [
		preload("res://tests/test_context_lifecycle.gd"),
		preload("res://tests/test_event_listeners.gd"),
		preload("res://tests/test_data_binding.gd"),
		preload("res://tests/test_error_recovery.gd"),
		preload("res://tests/test_document_management.gd"),
	]

	for script in tests:
		var instance = script.new()
		if instance.has_method("run"):
			instance.run(self)
		instance.free()

	_print_summary()
	quit(0 if _failed == 0 else 1)

func _print_summary():
	print("\n========================================")
	print("  RESULTS: %d passed, %d failed" % [_passed, _failed])
	print("========================================")
	if _errors.size() > 0:
		print("\nFailures:")
		for e in _errors:
			print("  FAIL: %s" % e)
	print("")

func assert_true(condition: bool, description: String):
	if condition:
		_passed += 1
		print("  PASS: %s" % description)
	else:
		_failed += 1
		_errors.append(description)
		print("  FAIL: %s" % description)

func assert_eq(actual, expected, description: String):
	if actual == expected:
		_passed += 1
		print("  PASS: %s" % description)
	else:
		_failed += 1
		var msg := "%s (got %s, expected %s)" % [description, str(actual), str(expected)]
		_errors.append(msg)
		print("  FAIL: %s" % msg)

func assert_not_null(value, description: String):
	assert_true(value != null, description)

func assert_gt(actual, threshold, description: String):
	if actual > threshold:
		_passed += 1
		print("  PASS: %s" % description)
	else:
		_failed += 1
		var msg := "%s (got %s, expected > %s)" % [description, str(actual), str(threshold)]
		_errors.append(msg)
		print("  FAIL: %s" % msg)
