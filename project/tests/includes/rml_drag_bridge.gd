class_name RmlDragBridge_GD
extends Control

signal drag_started(source_ctx: RmlContext, element_id: String, payload: Dictionary)
signal drag_dropped(source_ctx: RmlContext, target_ctx: RmlContext, drop_position: Vector2, payload: Dictionary)
signal drag_cancelled(source_ctx: RmlContext, payload: Dictionary)
signal drag_entered_context(target_ctx: RmlContext, payload: Dictionary)
signal drag_exited_context(target_ctx: RmlContext, payload: Dictionary)

enum State { IDLE, DRAGGING }

var _state: State = State.IDLE
var _source_ctx: RmlContext = null
var _source_element_id: String = ""
var _payload: Dictionary = {}
var _hover_ctx: RmlContext = null
var _ghost_offset: Vector2 = Vector2.ZERO

var _contexts: Array[RmlContext] = []
var _drag_sources: Array[Dictionary] = []
var _drop_targets: Array[Dictionary] = []

var _overlay_ctx: RmlContext = null
var _overlay_ready: bool = false
var _overlay_stylesheet_paths: Array[String] = []


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_setup_overlay()


func set_overlay_stylesheets(paths: Array[String]) -> void:
	_overlay_stylesheet_paths = paths


func register_drag_source(ctx: RmlContext, element_id: String, payload_builder: Callable = Callable()) -> void:
	_ensure_context(ctx)
	ctx.add_event_listener(element_id, "dragstart", _on_drag_start.bind(ctx))
	ctx.add_event_listener(element_id, "dragend", _on_drag_end.bind(ctx))
	_drag_sources.append({"ctx": ctx, "element_id": element_id, "payload_builder": payload_builder})


func register_drop_target(ctx: RmlContext, element_id: String) -> void:
	_ensure_context(ctx)
	_drop_targets.append({"ctx": ctx, "element_id": element_id})


func _ensure_context(ctx: RmlContext) -> void:
	if ctx not in _contexts:
		_contexts.append(ctx)


func _setup_overlay() -> void:
	_overlay_ctx = RmlContext.new()
	_overlay_ctx.name = "DragOverlay"
	_overlay_ctx.rml_context_name = "drag_overlay"
	_overlay_ctx.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_overlay_ctx.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_overlay_ctx)
	_overlay_ctx.visible = false
	print("[RmlDragBridge] Overlay context created, size: ", _overlay_ctx.size)


func _initialize_overlay_document() -> void:
	if _overlay_ready:
		return
	for path in _overlay_stylesheet_paths:
		_overlay_ctx.load_font_face(path)
	_overlay_ctx.load_document("res://tests/includes/drag_overlay.rml")
	_overlay_ready = true
	print("[RmlDragBridge] Overlay document loaded")


func _activate_overlay(source_ctx: RmlContext, element_id: String) -> void:
	if _overlay_ctx == null:
		return

	_initialize_overlay_document()
	_overlay_ctx.visible = true

	var outer_rml: String = source_ctx.get_element_outer_rml(element_id)
	print("[RmlDragBridge] Ghost outer RML: ", outer_rml.left(120), "...")
	if outer_rml.is_empty():
		push_warning("[RmlDragBridge] Could not extract outer RML for element: " + element_id)
		return

	_overlay_ctx.set_element_inner_rml("ghost-container", outer_rml)
	_ghost_offset = Vector2(-30, -30)
	print("[RmlDragBridge] Ghost injected, overlay visible: ", _overlay_ctx.visible, " size: ", _overlay_ctx.size)


func _deactivate_overlay() -> void:
	if _overlay_ctx == null:
		return
	_overlay_ctx.set_element_inner_rml("ghost-container", "")
	_overlay_ctx.visible = false


func _update_ghost_position(global_mouse: Vector2) -> void:
	if _overlay_ctx == null or not _overlay_ctx.visible:
		return

	var local_pos: Vector2 = global_mouse - _overlay_ctx.global_position + _ghost_offset
	var left_str: String = str(int(local_pos.x)) + "px"
	var top_str: String = str(int(local_pos.y)) + "px"
	_overlay_ctx.set_element_property("ghost-container", "left", left_str)
	_overlay_ctx.set_element_property("ghost-container", "top", top_str)


func _on_drag_start(event: Dictionary, source_ctx: RmlContext) -> void:
	if _state != State.IDLE:
		return

	_state = State.DRAGGING
	_source_ctx = source_ctx
	_source_element_id = event.get("target_id", "")
	_payload = _build_payload(source_ctx, _source_element_id, event)

	_activate_overlay(source_ctx, _source_element_id)

	drag_started.emit(source_ctx, _source_element_id, _payload)


func _on_drag_end(event: Dictionary, source_ctx: RmlContext) -> void:
	if _state != State.DRAGGING:
		return
	if source_ctx != _source_ctx:
		return

	var local_mouse := Vector2(event.get("mouse_x", 0), event.get("mouse_y", 0))
	var global_mouse := source_ctx.global_position + local_mouse

	var target_ctx := _find_context_at(global_mouse)

	if target_ctx != null and target_ctx != source_ctx:
		var drop_local := global_mouse - target_ctx.global_position
		drag_dropped.emit(source_ctx, target_ctx, drop_local, _payload)
	elif target_ctx == source_ctx:
		pass
	else:
		drag_cancelled.emit(source_ctx, _payload)

	if _hover_ctx != null:
		drag_exited_context.emit(_hover_ctx, _payload)
		_hover_ctx = null

	_deactivate_overlay()
	_state = State.IDLE
	_source_ctx = null
	_source_element_id = ""
	_payload = {}


func _process(_delta: float) -> void:
	if _state != State.DRAGGING:
		return
	if _source_ctx == null:
		return

	var global_mouse := _source_ctx.get_global_mouse_position()
	_update_ghost_position(global_mouse)

	var ctx_under := _find_context_at(global_mouse)

	if ctx_under != _source_ctx:
		if ctx_under != _hover_ctx:
			if _hover_ctx != null:
				drag_exited_context.emit(_hover_ctx, _payload)
			_hover_ctx = ctx_under
			if _hover_ctx != null:
				drag_entered_context.emit(_hover_ctx, _payload)
	else:
		if _hover_ctx != null:
			drag_exited_context.emit(_hover_ctx, _payload)
			_hover_ctx = null


func _find_context_at(global_pos: Vector2) -> RmlContext:
	for ctx in _contexts:
		if not ctx.is_visible_in_tree():
			continue
		var rect := ctx.get_global_rect()
		if rect.has_point(global_pos):
			return ctx
	return null


func _build_payload(ctx: RmlContext, element_id: String, event: Dictionary) -> Dictionary:
	for source in _drag_sources:
		if source["ctx"] == ctx and source["element_id"] == element_id:
			var builder: Callable = source["payload_builder"]
			if builder.is_valid():
				return builder.call(element_id, event)
			break

	var attrs := {}
	var handle: RmlElementHandle = ctx.get_element_by_id(element_id)
	if handle != null and handle.is_valid():
		attrs["tag"] = handle.get_tag_name()
		attrs["id"] = element_id
	return {"element_id": element_id, "source_attributes": attrs}


func cancel_drag() -> void:
	if _state == State.DRAGGING:
		drag_cancelled.emit(_source_ctx, _payload)
		if _hover_ctx != null:
			drag_exited_context.emit(_hover_ctx, _payload)
			_hover_ctx = null
		_deactivate_overlay()
		_state = State.IDLE
		_source_ctx = null
		_source_element_id = ""
		_payload = {}


func is_dragging() -> bool:
	return _state == State.DRAGGING


func get_payload() -> Dictionary:
	return _payload


func get_source_context() -> RmlContext:
	return _source_ctx
