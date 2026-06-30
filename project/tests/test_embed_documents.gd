extends SceneTree
## End-to-end test for embedded sub-documents (<embed-doc>, issue #56):
## - declarative <embed-doc src> mounts automatically on document load
## - the embedded document is a real subtree of the parent's DOM (shared layout
##   domain): its elements resolve via the context's get_element_by_id
## - the embedded document is made in-flow (position:relative), not a free
##   absolute root
## - imperative mount_embed / unmount_embed and the embed registry
## - the embedded <script> runs as its own GDScript instance with rml_context
##   injected, reachable via get_embedded_script (the parent->child data path)

const HOST := "res://tests/fixtures/embed/host.rml"
const WIDGET := "res://tests/fixtures/embed/widget.rml"
const GRID := "res://tests/fixtures/embed/grid.rml"

var _ctx: Node
var _phase := 0
var _fails := 0
var _ping := ""


func _initialize() -> void:
	_ctx = ClassDB.instantiate(&"RmlContext")
	root.add_child(_ctx)
	# Let _ready() create the RmlUi context before loading.
	create_timer(0.3).timeout.connect(_step)


func _step() -> void:
	match _phase:
		0:
			_ctx.call("load_document", HOST)
			_phase = 1
			create_timer(0.5).timeout.connect(_step)
		1:
			# Declarative embed mounted on load.
			_check("declarative embed mounted", bool(_ctx.call("is_embed_mounted", "w1")))
			var ids: PackedStringArray = _ctx.call("get_embedded_ids")
			_check("registry lists w1", ids.has("w1"))

			# The <embed-doc> host is a real element in the parent document.
			var host = _ctx.call("get_element_by_id", "w1")
			_check("host element resolves", host != null and host.call("is_valid"))
			if host != null and host.call("is_valid"):
				_check("host is <embed-doc>", str(host.call("get_tag_name")) == "embed-doc")

			# Shared layout domain: the embedded document's own element is found by
			# the context's id lookup — i.e. it is a subtree of the parent DOM, not
			# an isolated context.
			var inner = _ctx.call("get_element_by_id", "widget-root")
			_check("embedded element in shared DOM", inner != null and inner.call("is_valid"))
			if inner != null and inner.call("is_valid"):
				_check("embedded root is <body>", str(inner.call("get_tag_name")) == "body")
				_check("embedded content present", int(inner.call("get_child_count")) >= 1)
				# We override the document's default position:absolute so it lays
				# out in-flow as an ordinary child.
				_check("embed made in-flow", str(inner.call("get_property", "position")) == "relative")

			# The embedded <script> ran with an embed-scoped rml_context: its
			# on_load set `loaded` AND drove its own root by id through rml_context
			# (the data-loaded attribute resolved inside this embed's subtree).
			var w1_inst = _ctx.call("get_embedded_script", "w1")
			_check("embedded script ran with rml_context", w1_inst != null and bool(w1_inst.loaded))
			var w1_root = _ctx.call("get_embedded_element", "w1", "widget-root")
			_check("embed-scoped rml_context drove its own element",
				w1_root != null and w1_root.is_valid() and str(w1_root.get_attribute("data-loaded")) == "1")

			# In-embed interactivity: a button INSIDE the embed whose onclick handler
			# lives in the embed's OWN <script>, plus a signal the parent connected to.
			# Done while only w1 exists so the (intentionally shared) button id is
			# unambiguous.
			var w1 = _ctx.call("get_embedded_script", "w1")
			_check("w1 instance reachable", w1 != null)
			var self_signal := [false]
			if w1 != null:
				w1.pinged.connect(func(_m): self_signal[0] = true)
			var sbtn = _ctx.call("get_element_by_id", "w-self-btn")
			_check("in-embed button resolves", sbtn != null and sbtn.call("is_valid"))
			if sbtn != null and sbtn.call("is_valid"):
				sbtn.call("click")
				_check("in-embed onclick ran the embed's own handler", w1 != null and int(w1.self_clicked) == 1)
				_check("in-embed click emitted embed signal to parent", self_signal[0])
				if w1 != null:
					_check("in-embed handler mutated its own instance", int(w1.value) == 1)

			# Imperative mount of a second instance.
			var id2 = _ctx.call("mount_embed", "host-container", WIDGET, {"id": "w2"})
			_check("mount_embed returns id", str(id2) == "w2")
			_phase = 2
			create_timer(0.5).timeout.connect(_step)
		2:
			_check("imperative embed mounted", bool(_ctx.call("is_embed_mounted", "w2")))
			var ids2: PackedStringArray = _ctx.call("get_embedded_ids")
			_check("registry lists both embeds", ids2.size() == 2)

			# Reach the embed's <script> instance and drive it (parent->child path).
			var inst = _ctx.call("get_embedded_script", "w2")
			_check("get_embedded_script returns instance", inst != null)
			if inst != null:
				inst.pinged.connect(func(m): _ping = m)
				inst.ping()
				_check("embedded signal received", _ping == "pong")
				inst.bump()
				# bump() drove w2's own root by id; the attribute must land on w2's
				# widget-root (scoped), not w1's.
				var w2_root = _ctx.call("get_embedded_element", "w2", "widget-root")
				_check("embedded method reached rml_context",
					w2_root != null and w2_root.is_valid() and str(w2_root.get_attribute("data-bumped")) == "1")
				var w1_root2 = _ctx.call("get_embedded_element", "w1", "widget-root")
				_check("sibling embed's element untouched by w2's call",
					w1_root2 != null and w1_root2.is_valid() and str(w1_root2.get_attribute("data-bumped")) == "")

			# Scoped element lookup: w1 and w2 are the SAME .rml, so both contain an
			# element id "w-self-btn". get_element_by_id is context-global (first match
			# = w1's), but get_embedded_element is scoped to a single embed.
			var e1 = _ctx.call("get_embedded_script", "w1")
			var e2 = _ctx.call("get_embedded_script", "w2")
			var b1 = _ctx.call("get_embedded_element", "w1", "w-self-btn")
			var b2 = _ctx.call("get_embedded_element", "w2", "w-self-btn")
			_check("scoped lookup resolves w1's button", b1 != null and b1.call("is_valid"))
			_check("scoped lookup resolves w2's button", b2 != null and b2.call("is_valid"))
			if e1 != null and e2 != null and b2 != null and b2.call("is_valid"):
				var v1_before := int(e1.value)
				var v2_before := int(e2.value)
				b2.call("click")  # click w2's button via the SCOPED handle
				_check("scoped click hit the targeted embed (w2)", int(e2.value) == v2_before + 1)
				_check("scoped click left the sibling embed (w1) untouched", int(e1.value) == v1_before)
			_phase = 3
			create_timer(0.5).timeout.connect(_step)
		3:
			# Unmount removes the embed and its subtree.
			_check("unmount_embed succeeds", bool(_ctx.call("unmount_embed", "w2")))
			_check("embed no longer mounted", not bool(_ctx.call("is_embed_mounted", "w2")))
			var ids3: PackedStringArray = _ctx.call("get_embedded_ids")
			_check("registry back to one embed", ids3.size() == 1)
			var gone = _ctx.call("get_element_by_id", "w2")
			_check("unmounted host element gone", gone == null or not gone.call("is_valid"))
			_phase = 4
			create_timer(0.5).timeout.connect(_step)
		4:
			# Data-model namespacing: mount the SAME grid.rml twice with model
			# opt-in, feed each different data via its handle, prove isolation.
			var ga = _ctx.call("mount_embed", "host-container", GRID, {"id": "ga", "model": "grid"})
			var gb = _ctx.call("mount_embed", "host-container", GRID, {"id": "gb", "model": "grid"})
			_check("both grids mounted (same .rml)", str(ga) == "ga" and str(gb) == "gb")

			var da = _ctx.call("get_embedded_data", "ga")
			var db = _ctx.call("get_embedded_data", "gb")
			_check("data handle valid (ga)", da != null and da.is_valid())
			_check("data handle valid (gb)", db != null and db.is_valid())

			# Feed different data to each instance.
			da.set_array("cells", ["a1", "a2", "a3"])
			da.set_value("n", 3)
			db.set_array("cells", ["b1"])
			db.set_value("n", 1)

			# Data-level isolation: both author data-model "g", but independent.
			_check("ga cells isolated (3)", da.array_size("cells") == 3)
			_check("gb cells isolated (1)", db.array_size("cells") == 1)
			_check("ga scalar isolated (3)", int(da.get_value("n")) == 3)
			_check("gb scalar isolated (1)", int(db.get_value("n")) == 1)

			# The embed's own <script> received `var data` (its namespaced model).
			var sga = _ctx.call("get_embedded_script", "ga")
			_check("embed script got injected `data`", sga != null and int(sga.count()) == 3)
			sga.add("a4")  # the embed drives its own model through `data`
			_check("embed drove its own model", da.array_size("cells") == 4)
			_check("sibling grid untouched by embed", db.array_size("cells") == 1)

			_phase = 5
			create_timer(0.5).timeout.connect(_step)
		5:
			# Render isolation: data-for cloned the right number of rows per embed.
			# Verify by rendered CONTENT (robust against the hidden data-for template
			# element that RmlUi keeps as a child): each embed shows its own cells.
			var ga_rml: String = str(_ctx.call("get_embedded_element", "ga", "cells").call("get_outer_rml"))
			var gb_rml: String = str(_ctx.call("get_embedded_element", "gb", "cells").call("get_outer_rml"))
			_check("ga data-for rendered its cells", "a1" in ga_rml and "a4" in ga_rml)
			_check("gb data-for rendered its cell", "b1" in gb_rml)
			_check("embeds render isolated data", not ("a1" in gb_rml) and not ("b1" in ga_rml))

			print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
			quit(_fails)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
