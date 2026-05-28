extends Control

var _log_lines: Array[String] = []
var _hl_on: bool = false
var _active_on: bool = false
var _rml_set: bool = false

func _ready() -> void:
	var rml: RmlContext = $RmlContext
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
	rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")
	rml.load_document("res://addons/rmlui-godot/examples/basic/events/events.rml")

	# --- Click event listeners ---
	rml.add_event_listener("btn-a", "click", _on_button_click)
	rml.add_event_listener("btn-b", "click", _on_button_click)
	rml.add_event_listener("btn-c", "click", _on_button_click)

	# --- Hover event listeners ---
	rml.add_event_listener("hover-box", "mouseover", _on_hover_enter)
	rml.add_event_listener("hover-box", "mouseout", _on_hover_leave)

	# --- Class toggle buttons ---
	rml.add_event_listener("btn-toggle-hl", "click", _on_toggle_highlighted)
	rml.add_event_listener("btn-toggle-active", "click", _on_toggle_active)

	# --- Property modification buttons ---
	rml.add_event_listener("btn-color-red", "click", _on_color_red)
	rml.add_event_listener("btn-color-green", "click", _on_color_green)
	rml.add_event_listener("btn-color-blue", "click", _on_color_blue)
	rml.add_event_listener("btn-color-reset", "click", _on_color_reset)

	# --- Inner RML buttons ---
	rml.add_event_listener("btn-set-rml", "click", _on_set_rml)
	rml.add_event_listener("btn-clear-rml", "click", _on_clear_rml)


func _log(msg: String) -> void:
	_log_lines.append(msg)
	if _log_lines.size() > 20:
		_log_lines.remove_at(0)
	var rml: RmlContext = $RmlContext
	var html := ""
	for line in _log_lines:
		html += "<p>" + line + "</p>"
	rml.set_element_inner_rml("log-output", html)


func _on_button_click(event: Dictionary) -> void:
	var target_id: String = event.get("target_id", "unknown")
	rml_context().set_element_inner_rml("status-text", "Clicked: " + target_id)
	_log("click -> " + target_id)


func _on_hover_enter(_event: Dictionary) -> void:
	rml_context().set_element_class("hover-box", "hovered", true)
	_log("hover enter")


func _on_hover_leave(_event: Dictionary) -> void:
	rml_context().set_element_class("hover-box", "hovered", false)
	_log("hover leave")


func _on_toggle_highlighted(_event: Dictionary) -> void:
	_hl_on = !_hl_on
	rml_context().set_element_class("toggle-target", "highlighted", _hl_on)
	_log("highlighted = " + str(_hl_on))


func _on_toggle_active(_event: Dictionary) -> void:
	_active_on = !_active_on
	rml_context().set_element_class("toggle-target", "active", _active_on)
	_log("active = " + str(_active_on))


func _on_color_red(_event: Dictionary) -> void:
	rml_context().set_element_property("color-target", "background-color", "#aa3333")
	_log("color -> red")


func _on_color_green(_event: Dictionary) -> void:
	rml_context().set_element_property("color-target", "background-color", "#33aa33")
	_log("color -> green")


func _on_color_blue(_event: Dictionary) -> void:
	rml_context().set_element_property("color-target", "background-color", "#3333aa")
	_log("color -> blue")


func _on_color_reset(_event: Dictionary) -> void:
	rml_context().remove_element_property("color-target", "background-color")
	_log("color -> reset")


func _on_set_rml(_event: Dictionary) -> void:
	_rml_set = true
	rml_context().set_element_inner_rml("rml-target",
		"<p style='color: #00ff88;'>Dynamic RML content!</p><p style='color: #ffcc00;'>Second paragraph.</p>")
	_log("inner rml set")


func _on_clear_rml(_event: Dictionary) -> void:
	_rml_set = false
	rml_context().set_element_inner_rml("rml-target", "")
	_log("inner rml cleared")


func rml_context() -> RmlContext:
	return $RmlContext as RmlContext
