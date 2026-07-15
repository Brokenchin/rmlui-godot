extends Control
## Issue #61 — live A/B of overflow clipping for the addon drag/drop bridge.
##
## The embedded "bag" has an overflow scroll viewport over a tall slot grid. Move
## the cursor BELOW the bag's blue border, where rows past the viewport are laid
## out (and correctly NOT painted). Three readouts of "what is under the cursor":
##   • get_element_at_point      — RmlUi native hit-test  (always clip-correct)
##   • get_drop_target_at_point  — addon drag/drop bridge  (FIXED to clip in #61)
##   • raw box test (GDScript)   — replicates the OLD buggy _point_in_element
##
## Inside the bag all three agree. Below the bag, the raw box test still "hits"
## off-viewport slots (the bug); the other two correctly report nothing (fixed).

@onready var rml: Node = $Bag
@onready var status: Label = $Status

const N := 40


func _ready() -> void:
	# The embed mounts during document load; give it a moment, then register every
	# slot as a drop target so the bridge can resolve them.
	get_tree().create_timer(0.6).timeout.connect(_register)


func _noop(_a = null, _b = null) -> void:
	pass


func _register() -> void:
	for i in N:
		rml.call("register_drop_target", "slot_%d" % i, Callable(self, "_noop"))


## GDScript copy of the ORIGINAL _point_in_element (border box only, no clip) —
## kept here purely to visualise the leak the C++ fix removed.
func _raw_box_target(m: Vector2) -> String:
	for i in N:
		var h = rml.call("get_element_by_id", "slot_%d" % i)
		if h != null and h.is_valid():
			if Rect2(h.get_position(), h.get_size()).has_point(m):
				return "slot_%d" % i
	return ""


func _process(_delta: float) -> void:
	if rml == null:
		return
	var m: Vector2 = rml.get_local_mouse_position()
	var nat = rml.call("get_element_at_point", m)
	var native_id := str(nat.get_id()) if (nat != null and nat.is_valid()) else ""
	var drop := str(rml.call("get_drop_target_at_point", m))
	var raw := _raw_box_target(m)
	var leak := raw != "" and drop == "" and native_id == ""
	status.text = "cursor %s\n  get_element_at_point   (RmlUi native, clips) : '%s'\n  get_drop_target_at_point (addon drag/drop, FIXED): '%s'\n  raw box test            (OLD buggy logic)        : '%s'%s" % [
		m, native_id, drop, raw,
		"\n  -> below the bag: OLD logic leaks '%s' while the fixed bridge reports nothing." % raw if leak else ""]
