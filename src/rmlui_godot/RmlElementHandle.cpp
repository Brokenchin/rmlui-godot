#include "RmlElementHandle.hpp"

#include "GodotEventListener.hpp"

#include <RmlUi/Core/Element.h>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

godot::String RmlElementHandle::get_id() const {
	if (_element == nullptr) return {};
	return godot::String(_element->GetId().c_str());
}

void RmlElementHandle::set_id(const godot::String& id) {
	if (_element == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] RmlElementHandle::set_id — invalid element");
		return;
	}
	_element->SetId(Rml::String(id.utf8().get_data()));
}

godot::String RmlElementHandle::get_tag_name() const {
	if (_element == nullptr) return {};
	return godot::String(_element->GetTagName().c_str());
}

godot::String RmlElementHandle::get_inner_rml() const {
	if (_element == nullptr) return {};
	return godot::String(_element->GetInnerRML().c_str());
}

void RmlElementHandle::set_inner_rml(const godot::String& rml) {
	if (_element == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] RmlElementHandle::set_inner_rml — invalid element");
		return;
	}
	_element->SetInnerRML(Rml::String(rml.utf8().get_data()));
}

godot::String RmlElementHandle::get_attribute(const godot::String& name,
	const godot::String& default_value) const {
	if (_element == nullptr) return default_value;

	Rml::String rml_name(name.utf8().get_data());
	const Rml::Variant* attr = _element->GetAttribute(rml_name);
	if (attr == nullptr) return default_value;
	return godot::String(attr->Get<Rml::String>().c_str());
}

void RmlElementHandle::set_attribute(const godot::String& name, const godot::String& value) {
	if (_element == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] RmlElementHandle::set_attribute — invalid element");
		return;
	}
	_element->SetAttribute(
		Rml::String(name.utf8().get_data()),
		Rml::String(value.utf8().get_data()));
}

void RmlElementHandle::remove_attribute(const godot::String& name) {
	if (_element == nullptr) return;
	_element->RemoveAttribute(Rml::String(name.utf8().get_data()));
}

bool RmlElementHandle::has_attribute(const godot::String& name) const {
	if (_element == nullptr) return false;
	return _element->HasAttribute(Rml::String(name.utf8().get_data()));
}

godot::String RmlElementHandle::get_property(const godot::String& name) const {
	if (_element == nullptr) return {};

	const Rml::Property* prop = _element->GetProperty(Rml::String(name.utf8().get_data()));
	if (prop == nullptr) return {};
	return godot::String(prop->ToString().c_str());
}

bool RmlElementHandle::set_property(const godot::String& name, const godot::String& value) {
	if (_element == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] RmlElementHandle::set_property — invalid element");
		return false;
	}
	return _element->SetProperty(
		Rml::String(name.utf8().get_data()),
		Rml::String(value.utf8().get_data()));
}

void RmlElementHandle::remove_property(const godot::String& name) {
	if (_element == nullptr) return;
	_element->RemoveProperty(Rml::String(name.utf8().get_data()));
}

void RmlElementHandle::set_class(const godot::String& class_name, bool activate) {
	if (_element == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] RmlElementHandle::set_class — invalid element");
		return;
	}
	_element->SetClass(Rml::String(class_name.utf8().get_data()), activate);
}

bool RmlElementHandle::is_class_set(const godot::String& class_name) const {
	if (_element == nullptr) return false;
	return _element->IsClassSet(Rml::String(class_name.utf8().get_data()));
}

void RmlElementHandle::set_pseudo_class(const godot::String& pseudo_class, bool activate) {
	if (_element == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] RmlElementHandle::set_pseudo_class — invalid element");
		return;
	}
	_element->SetPseudoClass(Rml::String(pseudo_class.utf8().get_data()), activate);
}

bool RmlElementHandle::is_pseudo_class_set(const godot::String& pseudo_class) const {
	if (_element == nullptr) return false;
	return _element->IsPseudoClassSet(Rml::String(pseudo_class.utf8().get_data()));
}

int RmlElementHandle::get_child_count() const {
	if (_element == nullptr) return 0;
	return _element->GetNumChildren();
}

static Rml::String build_style_snapshot(Rml::Element* element) {
	static const char* visual_props[] = {
		"display", "width", "height",
		"background-color", "color",
		"font-size", "font-family", "text-align",
		"padding-top", "padding-right", "padding-bottom", "padding-left",
		"border-top-width", "border-right-width", "border-bottom-width", "border-left-width",
		"border-top-color", "border-right-color", "border-bottom-color", "border-left-color",
		"border-top-left-radius", "border-top-right-radius",
		"border-bottom-right-radius", "border-bottom-left-radius",
		"opacity",
	};

	Rml::String style;
	for (const char* name : visual_props) {
		const Rml::Property* prop = element->GetProperty(name);
		if (prop == nullptr) continue;
		if (!style.empty()) style += "; ";
		style += name;
		style += ": ";
		style += prop->ToString();
	}
	return style;
}

godot::String RmlElementHandle::get_outer_rml() const {
	if (_element == nullptr) return {};

	Rml::String result = "<";
	result += _element->GetTagName();

	for (const auto& [key, value] : _element->GetAttributes()) {
		if (key == "class" || key == "style") continue;
		result += " " + key + "=\"" + value.Get<Rml::String>() + "\"";
	}

	Rml::String class_names = _element->GetClassNames();
	if (!class_names.empty()) {
		result += " class=\"" + class_names + "\"";
	}

	Rml::String style = build_style_snapshot(_element);
	if (!style.empty()) {
		result += " style=\"" + style + "\"";
	}

	result += ">";
	result += _element->GetInnerRML();
	result += "</" + _element->GetTagName() + ">";

	return godot::String(result.c_str());
}

void RmlElementHandle::add_event_listener(const godot::String& event_type,
	const godot::Callable& callable, bool in_capture_phase) {

	if (_element == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] RmlElementHandle::add_event_listener — invalid element");
		return;
	}

	std::string type_str(event_type.utf8().get_data());
	auto* listener = new RmlGodot::GodotEventListener(callable, type_str);
	_element->AddEventListener(Rml::String(type_str), listener, in_capture_phase);
}

} // namespace RmlGodot
