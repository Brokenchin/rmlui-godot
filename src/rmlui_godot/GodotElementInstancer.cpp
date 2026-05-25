#include "GodotElementInstancer.hpp"

#include <RmlUi/Core/Element.h>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

// --- GodotCustomElement ---

GodotCustomElement::GodotCustomElement(const Rml::String& tag)
	: Rml::Element(tag)
{}

GodotCustomElement::~GodotCustomElement() = default;

void GodotCustomElement::FireOnCreate() {
	if (_create_fired || !_on_create.is_valid()) return;
	_create_fired = true;

	godot::Dictionary attrs;
	for (const auto& [key, value] : GetAttributes()) {
		attrs[godot::String(key.c_str())] = godot::String(value.Get<Rml::String>().c_str());
	}

	godot::Dictionary info;
	info["tag"] = godot::String(GetTagName().c_str());
	info["id"] = godot::String(GetId().c_str());
	info["attributes"] = attrs;

	_on_create.call(info);
}

void GodotCustomElement::OnChildAdd(Rml::Element* child) {
	Rml::Element::OnChildAdd(child);
	if (!_create_fired) {
		FireOnCreate();
	}
}

void GodotCustomElement::OnAttributeChange(const Rml::ElementAttributes& changed_attributes) {
	Rml::Element::OnAttributeChange(changed_attributes);

	if (!_on_attribute_change.is_valid()) return;

	godot::Dictionary changes;
	for (const auto& [key, value] : changed_attributes) {
		changes[godot::String(key.c_str())] = godot::String(value.Get<Rml::String>().c_str());
	}

	godot::Dictionary info;
	info["id"] = godot::String(GetId().c_str());
	info["tag"] = godot::String(GetTagName().c_str());
	info["changed"] = changes;

	_on_attribute_change.call(info);
}

// --- GodotElementInstancer ---

void GodotElementInstancer::register_tag(const std::string& tag, TagCallbacks callbacks) {
	_tag_callbacks[tag] = std::move(callbacks);
}

Rml::ElementPtr GodotElementInstancer::InstanceElement(
	Rml::Element* /*parent*/, const Rml::String& tag, const Rml::XMLAttributes& /*attributes*/) {

	auto* element = new GodotCustomElement(tag);

	std::string tag_str(tag.c_str());
	auto it = _tag_callbacks.find(tag_str);
	if (it != _tag_callbacks.end()) {
		element->SetOnCreateCallback(it->second.on_create);
		element->SetOnAttributeChangeCallback(it->second.on_attribute_change);
	}

	return Rml::ElementPtr(element);
}

void GodotElementInstancer::ReleaseElement(Rml::Element* element) {
	delete element;
}

} // namespace RmlGodot
