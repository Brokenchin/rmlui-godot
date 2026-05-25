#pragma once

#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/Element.h>
#include <godot_cpp/variant/callable.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace RmlGodot {

class GodotCustomElement final : public Rml::Element {
public:
	GodotCustomElement(const Rml::String& tag);
	~GodotCustomElement() override;

	void SetOnCreateCallback(godot::Callable callable) { _on_create = std::move(callable); }
	void SetOnAttributeChangeCallback(godot::Callable callable) { _on_attribute_change = std::move(callable); }

	void OnChildAdd(Rml::Element* child) override;
	void OnAttributeChange(const Rml::ElementAttributes& changed_attributes) override;

	void FireOnCreate();

private:
	godot::Callable _on_create;
	godot::Callable _on_attribute_change;
	bool _create_fired = false;
};

class GodotElementInstancer final : public Rml::ElementInstancer {
public:
	struct TagCallbacks {
		godot::Callable on_create;
		godot::Callable on_attribute_change;
	};

	void register_tag(const std::string& tag, TagCallbacks callbacks);

	Rml::ElementPtr InstanceElement(Rml::Element* parent, const Rml::String& tag,
		const Rml::XMLAttributes& attributes) override;
	void ReleaseElement(Rml::Element* element) override;

private:
	std::unordered_map<std::string, TagCallbacks> _tag_callbacks;
};

} // namespace RmlGodot
