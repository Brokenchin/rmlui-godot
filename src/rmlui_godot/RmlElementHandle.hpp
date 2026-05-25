#pragma once

#include "RmlGD.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

namespace Rml {
class Element;
}

namespace RmlGodot {

class RM_GD_CLASS(RmlElementHandle, godot::RefCounted, {

	godot::ClassDB::bind_method(godot::D_METHOD("is_valid"), &RmlElementHandle::is_valid);
	godot::ClassDB::bind_method(godot::D_METHOD("get_id"), &RmlElementHandle::get_id);
	godot::ClassDB::bind_method(godot::D_METHOD("set_id", "id"), &RmlElementHandle::set_id);
	godot::ClassDB::bind_method(godot::D_METHOD("get_tag_name"), &RmlElementHandle::get_tag_name);
	godot::ClassDB::bind_method(godot::D_METHOD("get_inner_rml"), &RmlElementHandle::get_inner_rml);
	godot::ClassDB::bind_method(godot::D_METHOD("set_inner_rml", "rml"), &RmlElementHandle::set_inner_rml);
	godot::ClassDB::bind_method(godot::D_METHOD("get_attribute", "name", "default_value"), &RmlElementHandle::get_attribute, DEFVAL(""));
	godot::ClassDB::bind_method(godot::D_METHOD("set_attribute", "name", "value"), &RmlElementHandle::set_attribute);
	godot::ClassDB::bind_method(godot::D_METHOD("remove_attribute", "name"), &RmlElementHandle::remove_attribute);
	godot::ClassDB::bind_method(godot::D_METHOD("has_attribute", "name"), &RmlElementHandle::has_attribute);
	godot::ClassDB::bind_method(godot::D_METHOD("get_property", "name"), &RmlElementHandle::get_property);
	godot::ClassDB::bind_method(godot::D_METHOD("set_property", "name", "value"), &RmlElementHandle::set_property);
	godot::ClassDB::bind_method(godot::D_METHOD("remove_property", "name"), &RmlElementHandle::remove_property);
	godot::ClassDB::bind_method(godot::D_METHOD("set_class", "class_name", "activate"), &RmlElementHandle::set_class);
	godot::ClassDB::bind_method(godot::D_METHOD("is_class_set", "class_name"), &RmlElementHandle::is_class_set);
	godot::ClassDB::bind_method(godot::D_METHOD("set_pseudo_class", "pseudo_class", "activate"), &RmlElementHandle::set_pseudo_class);
	godot::ClassDB::bind_method(godot::D_METHOD("is_pseudo_class_set", "pseudo_class"), &RmlElementHandle::is_pseudo_class_set);
	godot::ClassDB::bind_method(godot::D_METHOD("get_child_count"), &RmlElementHandle::get_child_count);
	godot::ClassDB::bind_method(godot::D_METHOD("get_outer_rml"), &RmlElementHandle::get_outer_rml);
	godot::ClassDB::bind_method(godot::D_METHOD("add_event_listener", "event_type", "callable", "in_capture_phase"), &RmlElementHandle::add_event_listener, DEFVAL(false));

});

public:
	RmlElementHandle() = default;
	~RmlElementHandle() override = default;

	void set_element(Rml::Element* element) { _element = element; }

	bool is_valid() const { return _element != nullptr; }
	godot::String get_id() const;
	void set_id(const godot::String& id);
	godot::String get_tag_name() const;
	godot::String get_inner_rml() const;
	void set_inner_rml(const godot::String& rml);

	godot::String get_attribute(const godot::String& name, const godot::String& default_value = "") const;
	void set_attribute(const godot::String& name, const godot::String& value);
	void remove_attribute(const godot::String& name);
	bool has_attribute(const godot::String& name) const;

	godot::String get_property(const godot::String& name) const;
	bool set_property(const godot::String& name, const godot::String& value);
	void remove_property(const godot::String& name);

	void set_class(const godot::String& class_name, bool activate);
	bool is_class_set(const godot::String& class_name) const;
	void set_pseudo_class(const godot::String& pseudo_class, bool activate);
	bool is_pseudo_class_set(const godot::String& pseudo_class) const;

	int get_child_count() const;
	godot::String get_outer_rml() const;

	void add_event_listener(const godot::String& event_type, const godot::Callable& callable, bool in_capture_phase = false);

private:
	Rml::Element* _element = nullptr;
};

} // namespace RmlGodot
