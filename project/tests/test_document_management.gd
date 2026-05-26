extends RefCounted

func run(runner) -> void:
	print("\n--- test_document_management ---")
	_test_get_loaded_documents_empty(runner)
	_test_reload_all_empty(runner)
	_test_context_info_uninitialized(runner)
	_test_context_info_fields(runner)
	_test_set_dp_ratio(runner)
	_test_set_text_render_mode(runner)
	_test_register_custom_element_no_callable(runner)

func _test_get_loaded_documents_empty(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "doc_empty"
	ctx.size = Vector2(400, 300)
	var docs: Array = ctx.get_loaded_documents()
	runner.assert_eq(docs.size(), 0, "get_loaded_documents returns empty array initially")
	ctx.free()

func _test_reload_all_empty(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "doc_reload_empty"
	ctx.size = Vector2(400, 300)
	# Should not crash even without initialized context
	ctx.reload_all_documents()
	runner.assert_true(true, "reload_all_documents on uninitialized context doesn't crash")
	ctx.free()

func _test_context_info_uninitialized(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "doc_info"
	ctx.size = Vector2(400, 300)
	var info: Dictionary = ctx.get_context_info()
	runner.assert_true(info.has("initialized"), "get_context_info returns dict with 'initialized'")
	runner.assert_eq(info["initialized"], false, "Uninitialized context reports initialized=false")
	ctx.free()

func _test_context_info_fields(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "doc_info_fields"
	ctx.size = Vector2(400, 300)
	var info: Dictionary = ctx.get_context_info()
	# Uninitialized should only have the initialized field
	runner.assert_true(info.size() >= 1, "get_context_info returns at least 1 field")
	ctx.free()

func _test_set_dp_ratio(runner) -> void:
	var ctx := RmlContext.new()
	ctx.dp_ratio = 2.0
	runner.assert_eq(ctx.dp_ratio, 2.0, "dp_ratio setter/getter roundtrip")
	ctx.dp_ratio = 0.5
	runner.assert_eq(ctx.dp_ratio, 0.5, "dp_ratio can be set to 0.5")
	ctx.free()

func _test_set_text_render_mode(runner) -> void:
	var ctx := RmlContext.new()
	ctx.text_render_mode = 0
	runner.assert_eq(ctx.text_render_mode, 0, "text_render_mode default is 0")
	ctx.text_render_mode = 3
	runner.assert_eq(ctx.text_render_mode, 3, "text_render_mode can be set to High Quality (3)")
	ctx.free()

func _test_register_custom_element_no_callable(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "doc_custom_elem"
	ctx.size = Vector2(400, 300)
	# Invalid callable should fail gracefully
	var result := ctx.register_custom_element("test-tag", Callable())
	runner.assert_true(not result, "register_custom_element with invalid callable returns false")
	ctx.free()
