#pragma once

#include <RmlUi/Core/EventListenerInstancer.h>

#include <string>
#include <unordered_map>
#include <functional>

namespace RmlGodot {

class GodotEventListenerInstancer final : public Rml::EventListenerInstancer {
public:
	using ListenerFactory = std::function<Rml::EventListener*(const Rml::String& value, Rml::Element* element)>;

	Rml::EventListener* InstanceEventListener(const Rml::String& value, Rml::Element* element) override;

	void register_factory(const std::string& prefix, ListenerFactory factory);

private:
	std::unordered_map<std::string, ListenerFactory> _factories;
};

} // namespace RmlGodot
