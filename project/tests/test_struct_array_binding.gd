extends SceneTree
## Struct/object array data binding (issue #26): bind_data_array with an array of
## dictionaries, then drive a data-for row that references the dict members
## (slot.icon, slot.count, slot.locked). Runs headless — data-for instancing and
## {{ }}/data-attr/data-if substitution happen during the context Update in
## _process, no pixel readback required; we inspect the resulting DOM via
## get_element_outer_rml.

const DOC := """<rml>
<head><style>body { width: 100%; height: 100%; } .slot { display: block; }</style></head>
<body data-model="bag">
	<div id="grid">
		<div class="slot" data-for="slot : slots">
			<img class="icon" data-attr-src="slot.icon" data-if="slot.icon != ''" />
			<span class="count" data-if="slot.count > 1">x{{ slot.count }}</span>
			<span class="name" data-class-locked="slot.locked">{{ slot.name }}</span>
		</div>
	</div>
</body>
</rml>"""

var _ctx
var _failed := 0
var _passed := 0

func _initialize() -> void:
	root.size = Vector2i(640, 480)
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	_ctx.size = Vector2(640, 480)
	# The context's _rml_context is created in _ready, which runs on the first
	# frame (after _initialize). Defer setup so the context exists first.
	create_timer(0.2).timeout.connect(_setup)

func _setup() -> void:
	# Model + struct array must exist before the document loads.
	_ctx.call("create_data_model", "bag")
	_ctx.call("bind_data_array", "bag", "slots", [
		{"icon": "texture://sword", "count": 5, "locked": false, "name": "Sword"},
		{"icon": "", "count": 1, "locked": true, "name": "Empty"},
	])
	_ctx.call("load_document_from_string", DOC, "memory://bag")
	# Let the document load + a couple of Update/layout passes run.
	create_timer(0.4).timeout.connect(_verify)

func _check(cond: bool, desc: String) -> void:
	if cond:
		_passed += 1
		print("  PASS: %s" % desc)
	else:
		_failed += 1
		print("  FAIL: %s" % desc)

func _verify() -> void:
	print("\n--- test_struct_array_binding ---")

	# Initial bind: two rows.
	_check(_ctx.call("get_data_array_size", "bag", "slots") == 2,
		"bound struct array reports size 2")

	var rml: String = _ctx.call("get_element_outer_rml", "grid")

	# Row 0 fields rendered from the dictionary.
	_check("texture://sword" in rml, "data-attr-src bound to slot.icon")
	_check("x5" in rml, "{{ slot.count }} numeric member rendered")
	_check("Sword" in rml, "{{ slot.name }} string member rendered")
	_check("Empty" in rml, "second row name rendered")
	# Numeric comparison on a struct member drives data-if both ways (RmlUi keeps
	# the element and toggles display:none): count==5 visible, count==1 hidden.
	_check('data-if="slot.count > 1">x5</span>' in rml,
		"data-if true for count>1 row (numeric member compare)")
	_check('style="display: none;">x1</span>' in rml,
		"data-if hides count span for count==1 row")
	# String comparison on a struct member: empty icon hides the img.
	_check('src="" style="display: none;"' in rml,
		"data-if=\"slot.icon != ''\" hides img for empty icon")

	# Mutators operate on the struct array.
	_ctx.call("push_data_array_item", "bag", "slots",
		{"icon": "texture://shield", "count": 9, "locked": false, "name": "Shield"})
	_check(_ctx.call("get_data_array_size", "bag", "slots") == 3, "push_data_array_item grows to 3")

	_ctx.call("set_data_array_item", "bag", "slots", 1,
		{"icon": "texture://gem", "count": 3, "locked": false, "name": "Gem"})
	_ctx.call("remove_data_array_item", "bag", "slots", 0)
	_check(_ctx.call("get_data_array_size", "bag", "slots") == 2, "remove_data_array_item shrinks to 2")

	_ctx.call("set_data_array", "bag", "slots", [{"name": "Solo", "count": 1, "icon": "", "locked": false}])
	_check(_ctx.call("get_data_array_size", "bag", "slots") == 1, "set_data_array replaces contents")

	_ctx.call("clear_data_array", "bag", "slots")
	_check(_ctx.call("get_data_array_size", "bag", "slots") == 0, "clear_data_array empties the array")

	print("  struct-array results: %d passed, %d failed" % [_passed, _failed])
	quit(0 if _failed == 0 else 1)
