#include "GodotEventListenerInstancer.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

Rml::EventListener* GodotEventListenerInstancer::InstanceEventListener(
	const Rml::String& value, Rml::Element* element) {

	for (const auto& [prefix, factory] : _factories) {
		if (value.rfind(prefix, 0) == 0) {
			return factory(value, element);
		}
	}

	godot::UtilityFunctions::push_warning(
		godot::String("[RmlUi] No listener factory for event value: ") +
		godot::String(value.c_str()));
	return nullptr;
}

void GodotEventListenerInstancer::register_factory(const std::string& prefix, ListenerFactory factory) {
	_factories[prefix] = std::move(factory);
}

} // namespace RmlGodot
