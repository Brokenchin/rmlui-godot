extends RefCounted

func run(runner) -> void:
	print("\n--- test_context_lifecycle ---")
	_test_rapid_create_destroy(runner)
	_test_create_destroy_50(runner)
	_test_context_count_tracking(runner)
	_test_double_cleanup(runner)
	_test_reenter_tree(runner)

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
	# Real tree exit before free (manually injecting NOTIFICATION_EXIT_TREE
	# into a node that never entered the tree crashes stock Godot — null
	# viewport in the engine's own exit handlers).
	var ctx := RmlContext.new()
	ctx.rml_context_name = "double_cleanup"
	ctx.size = Vector2(200, 200)
	var tree_root: Node = runner.root
	tree_root.add_child(ctx)
	tree_root.remove_child(ctx)
	ctx.free()
	runner.assert_true(true, "Double cleanup (tree exit + free) doesn't crash")

func _test_reenter_tree(runner) -> void:
	# Exit + re-enter must keep the context alive (editor tab switches and
	# runtime reparenting rely on this — regression for the EXIT_TREE
	# permadeath fixed in 5d074f8).
	var ctx := RmlContext.new()
	ctx.rml_context_name = "reenter"
	ctx.size = Vector2(200, 200)
	var tree_root: Node = runner.root
	tree_root.add_child(ctx)
	var info_before: Dictionary = ctx.get_context_info()
	tree_root.remove_child(ctx)
	tree_root.add_child(ctx)
	var info_after: Dictionary = ctx.get_context_info()
	runner.assert_eq(info_after.get("initialized", false),
		info_before.get("initialized", false), "Context survives tree exit/re-enter")
	tree_root.remove_child(ctx)
	ctx.free()
