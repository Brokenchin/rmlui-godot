#pragma once

#include <RmlUi/Core/EventListener.h>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <string>

namespace RmlGodot {

class GodotEventListener final : public Rml::EventListener {
public:
	GodotEventListener(godot::Callable callable, const std::string& event_type);

	void ProcessEvent(Rml::Event& event) override;
	void OnDetach(Rml::Element* element) override;

	const godot::Callable& get_callable() const { return _callable; }
	const std::string& get_event_type() const { return _event_type; }

private:
	godot::Callable _callable;
	std::string _event_type;

	static godot::Dictionary _build_event_dict(Rml::Event& event);
};

} // namespace RmlGodot
