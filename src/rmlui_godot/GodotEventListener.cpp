#include "GodotEventListener.hpp"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace RmlGodot {

GodotEventListener::GodotEventListener(godot::Callable callable, const std::string& event_type)
	: _callable(std::move(callable))
	, _event_type(event_type)
{}

void GodotEventListener::ProcessEvent(Rml::Event& event) {
	if (!_callable.is_valid()) return;

	godot::Dictionary dict = _build_event_dict(event);
	_callable.call(dict);
}

void GodotEventListener::OnDetach(Rml::Element* /*element*/) {
	// Prevent stale callable dispatch after element removal.
	_callable = godot::Callable();
}

godot::Dictionary GodotEventListener::_build_event_dict(Rml::Event& event) {
	godot::Dictionary dict;

	dict["type"] = godot::String(event.GetType().c_str());

	Rml::Element* target = event.GetTargetElement();
	if (target != nullptr) {
		dict["target_id"] = godot::String(target->GetId().c_str());
		dict["target_tag"] = godot::String(target->GetTagName().c_str());
	} else {
		dict["target_id"] = godot::String();
		dict["target_tag"] = godot::String();
	}

	Rml::Element* current = event.GetCurrentElement();
	if (current != nullptr) {
		dict["current_id"] = godot::String(current->GetId().c_str());
	}

	auto params = event.GetParameters();
	dict["mouse_x"] = static_cast<int64_t>(event.GetParameter<int>("mouse_x", 0));
	dict["mouse_y"] = static_cast<int64_t>(event.GetParameter<int>("mouse_y", 0));
	dict["button"] = static_cast<int64_t>(event.GetParameter<int>("button", -1));

	int key_id = event.GetParameter<int>("key_identifier", 0);
	dict["key_identifier"] = static_cast<int64_t>(key_id);

	int mods = event.GetParameter<int>("ctrl_key", 0)
		| (event.GetParameter<int>("shift_key", 0) << 1)
		| (event.GetParameter<int>("alt_key", 0) << 2)
		| (event.GetParameter<int>("meta_key", 0) << 3);
	dict["modifiers"] = static_cast<int64_t>(mods);

	dict["phase"] = static_cast<int64_t>(static_cast<int>(event.GetPhase()));

	auto* drag_el = static_cast<Rml::Element*>(event.GetParameter<void*>("drag_element", nullptr));
	if (drag_el != nullptr) {
		dict["drag_element_id"] = godot::String(drag_el->GetId().c_str());
		dict["drag_element_tag"] = godot::String(drag_el->GetTagName().c_str());
	}

	return dict;
}

} // namespace RmlGodot
