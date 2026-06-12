// RmlContext — input forwarding, gamepad navigation, drag & drop (see RmlContext.cpp for the TU map).
#include "RmlContext.hpp"
#include "RmlManager.hpp"
#include "RmlElementHandle.hpp"
#include "GodotEventListener.hpp"
#include "GodotFontInterface.hpp"
#include "GodotScriptDocument.hpp"

#include <algorithm>
#include <utility>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StyleSheetContainer.h>
#include <RmlUi/Debugger.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector4.hpp>

namespace RmlGodot {

void RmlContext::_gui_input(const godot::Ref<godot::InputEvent>& event) {
	if (_rml_context == nullptr) return;

	_forward_mouse_event(event);
	_forward_key_event(event);
	_render_dirty = true;
}

void RmlContext::toggle_debugger() {
	if (_rml_context == nullptr) return;
	static bool s_debugger_inited = false;
	if (!s_debugger_inited) {
		Rml::Debugger::Initialise(_rml_context);
		s_debugger_inited = true;
	}
	Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
	_render_dirty = true;
}

void RmlContext::set_input_actions(const godot::PackedStringArray& actions) {
	_input_actions = actions;
	_update_unhandled_input_processing();
}

void RmlContext::set_gamepad_navigation(bool enabled) {
	_gamepad_navigation = enabled;
	_update_unhandled_input_processing();
}

void RmlContext::_update_unhandled_input_processing() {
	// _unhandled_input only fires while enabled — keep it off unless watching
	// something, so contexts add zero per-event overhead by default.
	if (is_inside_tree()) {
		set_process_unhandled_input(_gamepad_navigation || !_input_actions.is_empty() ||
			_debugger_toggle_key != 0);
	}
}

// Godot's built-in ui_* actions (rebindable in the InputMap, gamepad bindings
// included by default) → the RmlUi keys that drive its built-in navigation:
// arrows = spatial nav via the nav-* properties, TAB = tab order,
// RETURN = Click() on the focused element, ESCAPE = forwarded for documents.
struct NavActionMap {
	const char* action;
	Rml::Input::KeyIdentifier key;
	int modifiers;
};
static constexpr NavActionMap k_nav_action_map[] = {
	{ "ui_up",         Rml::Input::KI_UP,     0 },
	{ "ui_down",       Rml::Input::KI_DOWN,   0 },
	{ "ui_left",       Rml::Input::KI_LEFT,   0 },
	{ "ui_right",      Rml::Input::KI_RIGHT,  0 },
	{ "ui_focus_next", Rml::Input::KI_TAB,    0 },
	{ "ui_focus_prev", Rml::Input::KI_TAB,    Rml::Input::KM_SHIFT },
	{ "ui_accept",     Rml::Input::KI_RETURN, 0 },
	{ "ui_cancel",     Rml::Input::KI_ESCAPE, 0 },
};

bool RmlContext::_process_navigation_input(const godot::Ref<godot::InputEvent>& event) {
	if (_rml_context == nullptr) return false;

	for (const auto& na : k_nav_action_map) {
		const godot::StringName action(na.action);
		if (!event->is_action_pressed(action)) continue;

		Rml::Input::KeyIdentifier key = na.key;
		int modifiers = na.modifiers;

		// Directional press with no real focus yet: grab the first tabbable
		// element instead of navigating from nowhere (arrows only work from
		// a focused element carrying a nav-* property).
		const bool is_arrow = (key == Rml::Input::KI_UP || key == Rml::Input::KI_DOWN ||
			key == Rml::Input::KI_LEFT || key == Rml::Input::KI_RIGHT);
		if (is_arrow) {
			Rml::Element* focus = _rml_context->GetFocusElement();
			if (focus == nullptr || focus->GetTagName() == "body" || focus->GetTagName() == "#root") {
				key = Rml::Input::KI_TAB;
				modifiers = 0;
			}
		}

		// ProcessKeyDown returns false when the UI consumed the key (focus
		// moved / element clicked) — claim the event so gameplay below the
		// UI doesn't also react to the same press.
		const bool propagated = _rml_context->ProcessKeyDown(key, modifiers);
		_rml_context->ProcessKeyUp(key, modifiers);
		_render_dirty = true;
		if (!propagated) {
			godot::Viewport* vp = get_viewport();
			if (vp != nullptr) {
				vp->set_input_as_handled();
			}
		}
		return true;
	}
	return false;
}

void RmlContext::_unhandled_input(const godot::Ref<godot::InputEvent>& event) {
	if (event.is_null()) return;

	// Debugger toggle — here and not in _gui_input: gui keyboard events only
	// reach a Control that HAS focus, and focus_mode defaults to click-grab.
	// Default F10: with Godot 4.5's embedded game window, F8/F9 are the
	// editor's Stop/Pause shortcuts and never reach the game.
	if (_debugger_toggle_key != 0) {
		auto* key = godot::Object::cast_to<godot::InputEventKey>(event.ptr());
		if (key != nullptr && key->is_pressed() && !key->is_echo() &&
			static_cast<int64_t>(key->get_keycode()) == _debugger_toggle_key) {
			toggle_debugger();
			godot::Viewport* vp = get_viewport();
			if (vp != nullptr) vp->set_input_as_handled();
			return;
		}
	}

	if (_gamepad_navigation) {
		_process_navigation_input(event);
	}

	if (_input_actions.is_empty()) return;

	for (int i = 0; i < _input_actions.size(); i++) {
		const godot::String action = _input_actions[i];
		bool pressed;
		if (event->is_action_pressed(action)) {
			pressed = true;
		} else if (event->is_action_released(action)) {
			pressed = false;
		} else {
			continue;
		}

		emit_signal("rml_input_action", action, pressed);

		// Dispatch into the documents' <script> blocks: first block across
		// all loaded documents defining _on_input_action(action, pressed).
		godot::Array args;
		args.append(action);
		args.append(pressed);
		for (auto& ld : _loaded_documents) {
			auto* doc = rmlui_dynamic_cast<GodotScriptDocument*>(ld.document);
			if (doc != nullptr && doc->dispatch_to_scripts("_on_input_action", args)) {
				break;
			}
		}
		_render_dirty = true;
	}
}

void RmlContext::register_drag_source(const godot::String& element_id,
	const godot::Callable& payload_builder, const godot::Callable& ghost_builder) {

	std::string id(element_id.utf8().get_data());
	for (const auto& src : _drag_sources) {
		if (src.element_id == id) {
			godot::UtilityFunctions::push_warning(
				godot::String("[RmlUi] Drag source already registered: ") + element_id);
			return;
		}
	}
	_drag_sources.push_back({id, payload_builder, ghost_builder});
}

void RmlContext::register_drop_target(const godot::String& element_id,
	const godot::Callable& drop_handler) {

	std::string id(element_id.utf8().get_data());
	for (const auto& tgt : _drop_targets) {
		if (tgt.element_id == id) {
			godot::UtilityFunctions::push_warning(
				godot::String("[RmlUi] Drop target already registered: ") + element_id);
			return;
		}
	}
	_drop_targets.push_back({id, drop_handler});
}

bool RmlContext::_point_in_element(Rml::Element* el, float x, float y) const {
	Rml::Vector2f offset = el->GetAbsoluteOffset(Rml::BoxArea::Border);
	Rml::Vector2f size = el->GetBox().GetSize(Rml::BoxArea::Border);
	return x >= offset.x && x <= offset.x + size.x
		&& y >= offset.y && y <= offset.y + size.y;
}

godot::Variant RmlContext::_get_drag_data(const godot::Vector2& p_at_position) {
	if (_rml_context == nullptr || _drag_sources.empty()) return {};

	for (const auto& source : _drag_sources) {
		Rml::Element* el = _find_element(godot::String(source.element_id.c_str()));
		if (el == nullptr) continue;

		if (!_point_in_element(el, p_at_position.x, p_at_position.y)) continue;

		godot::Dictionary payload;
		payload["_rml_source"] = true;
		payload["_element_id"] = godot::String(source.element_id.c_str());

		if (source.payload_builder.is_valid()) {
			godot::Variant result = source.payload_builder.call(
				godot::String(source.element_id.c_str()), p_at_position);
			if (result.get_type() == godot::Variant::DICTIONARY) {
				godot::Dictionary custom = result;
				godot::Array keys = custom.keys();
				for (int i = 0; i < keys.size(); i++) {
					payload[keys[i]] = custom[keys[i]];
				}
			}
		}

		_create_drag_ghost(source.element_id, source.ghost_builder);

		emit_signal("rml_drag_started",
			godot::String(source.element_id.c_str()), payload);

		return payload;
	}

	return {};
}

bool RmlContext::_can_drop_data(const godot::Vector2& p_at_position,
	const godot::Variant& /*p_data*/) const {

	if (_rml_context == nullptr || _drop_targets.empty()) return false;

	for (const auto& target : _drop_targets) {
		Rml::Element* el = _find_element(godot::String(target.element_id.c_str()));
		if (el == nullptr) continue;

		if (_point_in_element(el, p_at_position.x, p_at_position.y))
			return true;
	}

	return false;
}

void RmlContext::_drop_data(const godot::Vector2& p_at_position,
	const godot::Variant& p_data) {

	if (_rml_context == nullptr || _drop_targets.empty()) return;

	for (const auto& target : _drop_targets) {
		Rml::Element* el = _find_element(godot::String(target.element_id.c_str()));
		if (el == nullptr) continue;

		if (!_point_in_element(el, p_at_position.x, p_at_position.y)) continue;

		godot::String element_id(target.element_id.c_str());

		if (target.drop_handler.is_valid()) {
			target.drop_handler.call(element_id, p_data);
		}

		godot::Dictionary signal_data;
		if (p_data.get_type() == godot::Variant::DICTIONARY) {
			signal_data = p_data;
		}
		emit_signal("rml_drop_received", element_id, signal_data);
		return;
	}
}

Rml::String RmlContext::_build_ghost_rml(Rml::Element* el, int w, int h) {
	const auto& cv = el->GetComputedValues();

	auto fmt_color = [](Rml::Colourb c) -> std::string {
		char buf[32];
		snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%d)", c.red, c.green, c.blue, c.alpha);
		return buf;
	};

	auto fmt_px = [](float v) -> std::string {
		char buf[16];
		snprintf(buf, sizeof(buf), "%.0fpx", v);
		return buf;
	};

	std::string style;
	style += "display: block; overflow: hidden; ";
	style += "width: " + std::to_string(w) + "px; ";
	style += "height: " + std::to_string(h) + "px; ";
	style += "background-color: " + fmt_color(cv.background_color()) + "; ";
	style += "color: " + fmt_color(cv.color()) + "; ";
	style += "opacity: " + std::to_string(cv.opacity() * 0.8f) + "; ";
	style += "font-family: " + std::string(cv.font_family().c_str()) + "; ";
	style += "font-size: " + fmt_px(cv.font_size()) + "; ";

	float pt = cv.padding_top().value, pr = cv.padding_right().value;
	float pb = cv.padding_bottom().value, pl = cv.padding_left().value;
	if (pt > 0 || pr > 0 || pb > 0 || pl > 0) {
		style += "padding: " + fmt_px(pt) + " " + fmt_px(pr) + " "
			+ fmt_px(pb) + " " + fmt_px(pl) + "; ";
	}

	switch (cv.text_align()) {
		case Rml::Style::TextAlign::Center:  style += "text-align: center; "; break;
		case Rml::Style::TextAlign::Right:   style += "text-align: right; "; break;
		case Rml::Style::TextAlign::Justify: style += "text-align: justify; "; break;
		default: break;
	}

	float btw = cv.border_top_width(), brw = cv.border_right_width();
	float bbw = cv.border_bottom_width(), blw = cv.border_left_width();
	if (btw > 0) {
		style += "border-top-width: " + fmt_px(btw) + "; ";
		style += "border-top-color: " + fmt_color(cv.border_top_color()) + "; ";
	}
	if (brw > 0) {
		style += "border-right-width: " + fmt_px(brw) + "; ";
		style += "border-right-color: " + fmt_color(cv.border_right_color()) + "; ";
	}
	if (bbw > 0) {
		style += "border-bottom-width: " + fmt_px(bbw) + "; ";
		style += "border-bottom-color: " + fmt_color(cv.border_bottom_color()) + "; ";
	}
	if (blw > 0) {
		style += "border-left-width: " + fmt_px(blw) + "; ";
		style += "border-left-color: " + fmt_color(cv.border_left_color()) + "; ";
	}

	Rml::String inner_rml = el->GetInnerRML();

	return "<rml><head><style>body { margin: 0; padding: 0; }</style></head><body>"
		"<div style=\"" + Rml::String(style) + "\">" + inner_rml + "</div>"
		"</body></rml>";
}

void RmlContext::_create_drag_ghost(const std::string& source_element_id,
	const godot::Callable& ghost_builder) {

	Rml::Element* el = _find_element(godot::String(source_element_id.c_str()));
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Drag ghost: source element not found");
		return;
	}

	Rml::Vector2f el_size = el->GetBox().GetSize(Rml::BoxArea::Border);
	int w = std::max(1, static_cast<int>(el_size.x));
	int h = std::max(1, static_cast<int>(el_size.y));
	godot::Vector2 size_vec(w, h);

	godot::String ghost_rml;
	if (ghost_builder.is_valid()) {
		godot::Variant result = ghost_builder.call(
			godot::String(source_element_id.c_str()), size_vec);
		if (result.get_type() == godot::Variant::STRING) {
			ghost_rml = result;
		}
	}

	if (ghost_rml.is_empty()) {
		ghost_rml = godot::String(_build_ghost_rml(el, w, h).c_str());
	}

	RmlContext* ghost = memnew(RmlContext);
	ghost->_context_name = "drag_ghost";
	ghost->set_custom_minimum_size(size_vec);
	ghost->set_size(size_vec);
	ghost->set_mouse_filter(MOUSE_FILTER_IGNORE);

	auto* manager = RmlGodot::RmlManager::get_singleton();
	manager->ensure_initialized();

	static int s_ghost_counter = 0;
	std::string ghost_name = "drag_ghost_" + std::to_string(s_ghost_counter++);
	ghost->_rml_context = Rml::CreateContext(Rml::String(ghost_name),
		Rml::Vector2i(w, h), &ghost->_render_interface);

	if (ghost->_rml_context == nullptr) {
		memdelete(ghost);
		godot::UtilityFunctions::push_warning("[RmlUi] Drag ghost: failed to create context");
		return;
	}
	ghost->_rml_context->SetDensityIndependentPixelRatio(_dp_ratio);

	for (const auto& [name, tex] : _render_interface.get_registered_textures()) {
		ghost->_render_interface.register_texture(name, tex);
	}

	Rml::ElementDocument* doc = ghost->_rml_context->LoadDocumentFromMemory(
		Rml::String(ghost_rml.utf8().get_data()));
	if (doc == nullptr) {
		memdelete(ghost);
		godot::UtilityFunctions::push_warning("[RmlUi] Drag ghost: document load failed");
		return;
	}

	doc->Show();
	ghost->_rml_context->Update();

	set_drag_preview(ghost);
}

// --- Phase 8b: Dev tools & extended document management ---

static int godot_button_to_rml(godot::MouseButton button) {
	switch (button) {
		case godot::MOUSE_BUTTON_LEFT:   return 0;
		case godot::MOUSE_BUTTON_RIGHT:  return 1;
		case godot::MOUSE_BUTTON_MIDDLE: return 2;
		default: return 3;
	}
}

static int godot_modifiers_to_rml(const godot::Ref<godot::InputEvent>& event) {
	int mod = 0;
	auto* key_event = godot::Object::cast_to<godot::InputEventWithModifiers>(event.ptr());
	if (key_event == nullptr) return mod;

	if (key_event->is_ctrl_pressed())  mod |= Rml::Input::KM_CTRL;
	if (key_event->is_shift_pressed()) mod |= Rml::Input::KM_SHIFT;
	if (key_event->is_alt_pressed())   mod |= Rml::Input::KM_ALT;
	if (key_event->is_meta_pressed())  mod |= Rml::Input::KM_META;
	return mod;
}

void RmlContext::_forward_mouse_event(const godot::Ref<godot::InputEvent>& event) {
	auto* motion = godot::Object::cast_to<godot::InputEventMouseMotion>(event.ptr());
	if (motion != nullptr) {
		godot::Vector2 pos = motion->get_position();
		_rml_context->ProcessMouseMove(
			static_cast<int>(pos.x), static_cast<int>(pos.y),
			godot_modifiers_to_rml(event));
		return;
	}

	const auto* button = godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr());

	if (button == nullptr)
		return;

	const godot::Vector2 pos = button->get_position();
	_rml_context->ProcessMouseMove(
		static_cast<int>(pos.x), static_cast<int>(pos.y),
		godot_modifiers_to_rml(event));

	// Scroll wheel
	if (button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_UP && button->is_pressed()) {
		_rml_context->ProcessMouseWheel(Rml::Vector2f(0, -1), godot_modifiers_to_rml(event));
		return;
	}
	if (button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_DOWN && button->is_pressed()) {
		_rml_context->ProcessMouseWheel(Rml::Vector2f(0, 1), godot_modifiers_to_rml(event));
		return;
	}

	const int rml_button = godot_button_to_rml(button->get_button_index());
	if (button->is_pressed()) {
		_rml_context->ProcessMouseButtonDown(rml_button, godot_modifiers_to_rml(event));
	} else {
		_rml_context->ProcessMouseButtonUp(rml_button, godot_modifiers_to_rml(event));
	}


}

void RmlContext::_forward_key_event(const godot::Ref<godot::InputEvent>& event) {
	auto* key = godot::Object::cast_to<godot::InputEventKey>(event.ptr());
	if (key == nullptr) return;

	// Minimal key mapping — extend as needed.
	Rml::Input::KeyIdentifier rml_key = Rml::Input::KI_UNKNOWN;

	//we could do here static constexpr mapping to keys, similar to what I do for my game engine.
	// static constexpr std::array<Rml::Input::KeyIdentifier, 26> s_key_map = {
	// 	Rml::Input::KI_A, Rml::Input::KI_B, Rml::Input::KI_C, Rml::Input::KI_D, Rml::Input::KI_E,
	// 	Rml::Input::KI_F, Rml::Input::KI_G, Rml::Input::KI_H, Rml::Input::KI_I, Rml::Input::KI_J,
	// 	Rml::Input::KI_K, Rml::Input::KI_L, Rml::Input::KI_M, Rml::Input::KI_N, Rml::Input::KI_O,
	// 	Rml::Input::KI_P, Rml::Input::KI_Q, Rml::Input::KI_R, Rml::Input::KI_S, Rml::Input::KI_T,
	// 	Rml::Input::KI_U, Rml::Input::KI_V, Rml::Input::KI_W, Rml::Input::KI_X, Rml::Input::KI_Y,
	// 	Rml::Input::KI_Z
	// };

	//like map that is queried with godot keys -> returns the KI relevant key

	// not quite like the above it needs to be but it is still a simple mapping that covers all letters and digits.
	// like direct mapping to godot -> Rml. it needs double map. and we cannot do that directly godot KEY backspace is 4194308 large number.
	auto keycode = key->get_keycode();
	if (keycode >= godot::KEY_A && keycode <= godot::KEY_Z) {
		rml_key = static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_A + (static_cast<int>(keycode) - static_cast<int>(godot::KEY_A)));
	} else if (keycode >= godot::KEY_0 && keycode <= godot::KEY_9) {
		rml_key = static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_0 + (static_cast<int>(keycode) - static_cast<int>(godot::KEY_0)));
	} else if (keycode == godot::KEY_ENTER || keycode == godot::KEY_KP_ENTER) {
		rml_key = Rml::Input::KI_RETURN;
	} else if (keycode == godot::KEY_BACKSPACE) {
		rml_key = Rml::Input::KI_BACK;
	} else if (keycode == godot::KEY_TAB) {
		rml_key = Rml::Input::KI_TAB;
	} else if (keycode == godot::KEY_ESCAPE) {
		rml_key = Rml::Input::KI_ESCAPE;
	}
	// (The debugger toggle key is handled in _unhandled_input — _gui_input
	// only receives keys when the Control has focus.)

	const int modifiers = godot_modifiers_to_rml(event);

	if (key->is_pressed()) {
		_rml_context->ProcessKeyDown(rml_key, modifiers);

		// Forward printable characters for text input.
		if (key->get_unicode() > 0) {
			_rml_context->ProcessTextInput(static_cast<Rml::Character>(key->get_unicode()));
		}
	} else {
		_rml_context->ProcessKeyUp(rml_key, modifiers);
	}
}

} // namespace RmlGodot
