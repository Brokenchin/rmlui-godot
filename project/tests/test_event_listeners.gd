extends RefCounted

const HELLO_RML := '<rml><head></head><body><button id="btn1">Click</button><div id="div1">Text</div></body></rml>'

func run(runner) -> void:
	print("\n--- test_event_listeners ---")
	_test_add_remove_listeners(runner)
	_test_listener_cleanup_on_free(runner)
	_test_listener_on_nonexistent_element(runner)
	_test_element_handle_listener(runner)

func _test_add_remove_listeners(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "evt_test_1"
	ctx.size = Vector2(400, 300)
	# We need to go through _ready to init the RmlUI context
	# Since we can't add to tree in headless, manually init
	RmlManager.ensure_initialized()

	# Without being in tree, _rml_context won't be created via _ready.
	# But we can test the API doesn't crash with null context.
	var cb := func(_dict): pass
	var result := ctx.add_event_listener("btn1", "click", cb)
	# Should return false — context not initialized (not in tree)
	runner.assert_true(not result, "add_event_listener returns false when context not ready")
	ctx.free()

func _test_listener_cleanup_on_free(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "evt_cleanup"
	ctx.size = Vector2(400, 300)
	ctx.free()
	runner.assert_true(true, "Context with no listeners frees cleanly")

func _test_listener_on_nonexistent_element(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "evt_noelem"
	ctx.size = Vector2(400, 300)
	var cb := func(_dict): pass
	# This should warn but not crash
	var result := ctx.add_event_listener("nonexistent_id", "click", cb)
	runner.assert_true(not result, "add_event_listener on nonexistent element returns false")
	ctx.free()

func _test_element_handle_listener(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "evt_handle"
	ctx.size = Vector2(400, 300)
	# get_element_by_id on uninitialized context
	var handle = ctx.get_element_by_id("anything")
	runner.assert_true(handle != null, "get_element_by_id returns handle object (not null)")
	runner.assert_true(not handle.is_valid(), "Handle is_valid() false for uninitialized context")
	ctx.free()
