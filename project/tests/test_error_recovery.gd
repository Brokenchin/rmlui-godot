extends RefCounted

func run(runner) -> void:
	print("\n--- test_error_recovery ---")
	_test_load_missing_document(runner)
	_test_load_empty_path(runner)
	_test_load_missing_font(runner)
	_test_load_null_font_resource(runner)
	_test_register_null_texture(runner)
	_test_unregister_nonexistent_texture(runner)
	_test_inject_invalid_rcss(runner)
	_test_unload_untracked_document(runner)
	_test_reload_unloaded_document(runner)
	_test_manager_info(runner)

func _test_load_missing_document(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_missing_doc"
	ctx.size = Vector2(400, 300)
	# Context not initialized, should fail gracefully
	ctx.load_document("res://nonexistent/path.rml")
	runner.assert_true(true, "load_document with missing file doesn't crash")
	ctx.free()

func _test_load_empty_path(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_empty_path"
	ctx.size = Vector2(400, 300)
	ctx.load_document("")
	runner.assert_true(true, "load_document with empty path doesn't crash")
	ctx.free()

func _test_load_missing_font(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_missing_font"
	ctx.size = Vector2(400, 300)
	var result := ctx.load_font_face("res://nonexistent/font.ttf")
	runner.assert_true(not result, "load_font_face with missing file returns false")
	ctx.free()

func _test_load_null_font_resource(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_null_font"
	ctx.size = Vector2(400, 300)
	var result := ctx.load_font_resource(null)
	runner.assert_true(not result, "load_font_resource with null returns false")
	ctx.free()

func _test_register_null_texture(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_null_tex"
	ctx.size = Vector2(400, 300)
	var result := ctx.register_texture("test", null)
	runner.assert_true(not result, "register_texture with null texture returns false")
	ctx.free()

func _test_unregister_nonexistent_texture(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_unreg_tex"
	ctx.size = Vector2(400, 300)
	var result := ctx.unregister_texture("nonexistent")
	runner.assert_true(not result, "unregister_texture on nonexistent returns false")
	ctx.free()

func _test_inject_invalid_rcss(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_bad_rcss"
	ctx.size = Vector2(400, 300)
	# Without initialized context, should fail gracefully
	var result := ctx.inject_stylesheet("this is { not: valid rcss ;;;")
	runner.assert_true(not result, "inject_stylesheet on uninitialized context returns false")
	ctx.free()

func _test_unload_untracked_document(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_unload"
	ctx.size = Vector2(400, 300)
	var result := ctx.unload_document("res://never_loaded.rml")
	runner.assert_true(not result, "unload_document on untracked path returns false")
	ctx.free()

func _test_reload_unloaded_document(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "err_reload"
	ctx.size = Vector2(400, 300)
	var result := ctx.reload_document("res://never_loaded.rml")
	runner.assert_true(not result, "reload_document on unloaded path returns false")
	ctx.free()

func _test_manager_info(runner) -> void:
	var info: Dictionary = RmlManager.get_info()
	runner.assert_true(info.has("initialized"), "RmlManager.get_info() returns dict with 'initialized'")
	runner.assert_true(info.has("context_count"), "RmlManager.get_info() returns dict with 'context_count'")
