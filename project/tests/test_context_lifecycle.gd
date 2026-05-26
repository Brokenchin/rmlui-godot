extends RefCounted

func run(runner) -> void:
	print("\n--- test_context_lifecycle ---")
	_test_rapid_create_destroy(runner)
	_test_create_destroy_50(runner)
	_test_context_count_tracking(runner)
	_test_double_cleanup(runner)

func _test_rapid_create_destroy(runner) -> void:
	for i in range(10):
		var ctx := RmlContext.new()
		ctx.rml_context_name = "rapid_%d" % i
		ctx.size = Vector2(200, 200)
		ctx.free()
	runner.assert_true(true, "Rapid create/destroy 10x without crash")

func _test_create_destroy_50(runner) -> void:
	var contexts: Array[RmlContext] = []
	for i in range(50):
		var ctx := RmlContext.new()
		ctx.rml_context_name = "batch_%d" % i
		ctx.size = Vector2(100, 100)
		contexts.append(ctx)

	var count_before: int = RmlManager.get_context_count()
	runner.assert_eq(count_before, 50, "50 contexts tracked after batch creation")

	for ctx in contexts:
		ctx.free()
	contexts.clear()

	var count_after: int = RmlManager.get_context_count()
	runner.assert_eq(count_after, 0, "Context count returns to 0 after freeing all")

func _test_context_count_tracking(runner) -> void:
	var initial_count: int = RmlManager.get_context_count()
	var ctx := RmlContext.new()
	var after_create: int = RmlManager.get_context_count()
	runner.assert_eq(after_create, initial_count + 1, "Count increments on create")
	ctx.free()
	var after_free: int = RmlManager.get_context_count()
	runner.assert_eq(after_free, initial_count, "Count decrements on free")

func _test_double_cleanup(runner) -> void:
	var ctx := RmlContext.new()
	ctx.rml_context_name = "double_cleanup"
	ctx.size = Vector2(200, 200)
	# Calling notification manually to simulate EXIT_TREE before free
	ctx.notification(ctx.NOTIFICATION_EXIT_TREE)
	ctx.free()
	runner.assert_true(true, "Double cleanup (EXIT_TREE + free) doesn't crash")
