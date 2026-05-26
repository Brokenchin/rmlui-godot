extends RefCounted

func run(runner) -> void:
	print("\n--- test_data_binding ---")
	_test_data_model_without_context(runner)
	_test_data_model_create_destroy(runner)
	_test_array_binding_without_context(runner)
	_test_update_nonexistent_model(runner)

func _test_data_model_without_context(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "dm_nocontext"
	ctx.size = Vector2(400, 300)
	# Context not ready (no _ready call), data model creation should fail gracefully
	var result := ctx.create_data_model("test_model")
	runner.assert_true(not result, "create_data_model fails gracefully on uninitialized context")
	ctx.free()

func _test_data_model_create_destroy(runner) -> void:
	# Create context, immediately free — data models cleaned up
	var ctx := RmlContext.new()
	ctx.rml_context_name = "dm_lifecycle"
	ctx.size = Vector2(400, 300)
	# Even without a real RmlUI context, freeing should not crash
	ctx.free()
	runner.assert_true(true, "Context with no data models frees cleanly")

func _test_array_binding_without_context(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "arr_nocontext"
	ctx.size = Vector2(400, 300)
	var result := ctx.bind_data_array("model", "items", ["a", "b", "c"])
	runner.assert_true(not result, "bind_data_array fails gracefully on uninitialized context")
	ctx.free()

func _test_update_nonexistent_model(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "dm_nonexist"
	ctx.size = Vector2(400, 300)
	# Should push a warning but not crash
	ctx.update_data_model("nonexistent", {"key": "value"})
	runner.assert_true(true, "update_data_model on nonexistent model doesn't crash")
	ctx.free()
