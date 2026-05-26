#pragma once

#include "RmlGD.hpp"
#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "GodotRenderInterface.hpp"
#include "RmlElementHandle.hpp"

#include <unordered_map>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Variant.h>

namespace Rml {
class Context;
class Element;
class ElementDocument;
class EventListener;
}

namespace RmlGodot {

class RM_GD_CLASS(RmlContext, godot::Control, {

	godot::ClassDB::bind_method(godot::D_METHOD("load_document", "path"), &RmlContext::load_document);
	godot::ClassDB::bind_method(godot::D_METHOD("reload_document", "path"), &RmlContext::reload_document);
	godot::ClassDB::bind_method(godot::D_METHOD("reload_all_documents"), &RmlContext::reload_all_documents);
	godot::ClassDB::bind_method(godot::D_METHOD("get_loaded_documents"), &RmlContext::get_loaded_documents);
	godot::ClassDB::bind_method(godot::D_METHOD("load_font_face", "path"), &RmlContext::load_font_face);
	godot::ClassDB::bind_method(godot::D_METHOD("load_font_resource", "font"), &RmlContext::load_font_resource);
	godot::ClassDB::bind_method(godot::D_METHOD("get_rml_context_name"), &RmlContext::get_rml_context_name);
	godot::ClassDB::bind_method(godot::D_METHOD("set_rml_context_name", "name"), &RmlContext::set_rml_context_name);
	godot::ClassDB::bind_method(godot::D_METHOD("get_dp_ratio"), &RmlContext::get_dp_ratio);
	godot::ClassDB::bind_method(godot::D_METHOD("set_dp_ratio", "ratio"), &RmlContext::set_dp_ratio);
	godot::ClassDB::bind_method(godot::D_METHOD("create_data_model", "model_name"), &RmlContext::create_data_model);
	godot::ClassDB::bind_method(godot::D_METHOD("bind_data_variable", "model_name", "variable_name", "initial_value"), &RmlContext::bind_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("set_data_variable", "model_name", "variable_name", "value"), &RmlContext::set_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("get_data_variable", "model_name", "variable_name"), &RmlContext::get_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("bind_data_event", "model_name", "event_name", "callable"), &RmlContext::bind_data_event);
	godot::ClassDB::bind_method(godot::D_METHOD("dirty_data_variable", "model_name", "variable_name"), &RmlContext::dirty_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("dirty_all_variables", "model_name"), &RmlContext::dirty_all_variables);
	godot::ClassDB::bind_method(godot::D_METHOD("create_data_model_from_dict", "model_name", "variables"), &RmlContext::create_data_model_from_dict);
	godot::ClassDB::bind_method(godot::D_METHOD("update_data_model", "model_name", "variables"), &RmlContext::update_data_model);

	// Phase 3: Array data binding
	godot::ClassDB::bind_method(godot::D_METHOD("bind_data_array", "model_name", "array_name", "initial_array"), &RmlContext::bind_data_array);
	godot::ClassDB::bind_method(godot::D_METHOD("set_data_array", "model_name", "array_name", "array"), &RmlContext::set_data_array);
	godot::ClassDB::bind_method(godot::D_METHOD("push_data_array_item", "model_name", "array_name", "value"), &RmlContext::push_data_array_item);
	godot::ClassDB::bind_method(godot::D_METHOD("remove_data_array_item", "model_name", "array_name", "index"), &RmlContext::remove_data_array_item);
	godot::ClassDB::bind_method(godot::D_METHOD("set_data_array_item", "model_name", "array_name", "index", "value"), &RmlContext::set_data_array_item);
	godot::ClassDB::bind_method(godot::D_METHOD("get_data_array_size", "model_name", "array_name"), &RmlContext::get_data_array_size);
	godot::ClassDB::bind_method(godot::D_METHOD("clear_data_array", "model_name", "array_name"), &RmlContext::clear_data_array);

	// Phase 5: Custom element instancers
	godot::ClassDB::bind_method(godot::D_METHOD("register_custom_element", "tag_name", "on_create", "on_attribute_change"), &RmlContext::register_custom_element, DEFVAL(godot::Callable()));

	// Phase 1: DOM events & element access
	godot::ClassDB::bind_method(godot::D_METHOD("add_event_listener", "element_id", "event_type", "callable", "in_capture_phase"), &RmlContext::add_event_listener, DEFVAL(false));
	godot::ClassDB::bind_method(godot::D_METHOD("remove_event_listeners", "element_id", "event_type"), &RmlContext::remove_event_listeners);
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_by_id", "id"), &RmlContext::get_element_by_id);
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_property", "element_id", "property", "value"), &RmlContext::set_element_property);
	godot::ClassDB::bind_method(godot::D_METHOD("remove_element_property", "element_id", "property"), &RmlContext::remove_element_property);
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_class", "element_id", "class_name", "activate"), &RmlContext::set_element_class);
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_inner_rml", "element_id", "rml"), &RmlContext::set_element_inner_rml);
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_outer_rml", "element_id"), &RmlContext::get_element_outer_rml);
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_attribute", "element_id", "attribute", "default_value"), &RmlContext::get_element_attribute, DEFVAL(""));
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_attribute", "element_id", "attribute", "value"), &RmlContext::set_element_attribute);

	// Texture registration
	godot::ClassDB::bind_method(godot::D_METHOD("register_texture", "name", "texture"), &RmlContext::register_texture);
	godot::ClassDB::bind_method(godot::D_METHOD("unregister_texture", "name"), &RmlContext::unregister_texture);

	// A4: Drag-and-drop (gd_drag interop)
	godot::ClassDB::bind_method(godot::D_METHOD("register_drag_source", "element_id", "payload_builder", "ghost_builder"), &RmlContext::register_drag_source, DEFVAL(godot::Callable()), DEFVAL(godot::Callable()));
	godot::ClassDB::bind_method(godot::D_METHOD("register_drop_target", "element_id", "drop_handler"), &RmlContext::register_drop_target, DEFVAL(godot::Callable()));

	ADD_SIGNAL(godot::MethodInfo("rml_drag_started",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "payload")));
	ADD_SIGNAL(godot::MethodInfo("rml_drop_received",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "data")));

	// Phase 8b: Dev tools & extended document management
	godot::ClassDB::bind_method(godot::D_METHOD("inject_stylesheet", "rcss_string"), &RmlContext::inject_stylesheet);
	godot::ClassDB::bind_method(godot::D_METHOD("unload_document", "path"), &RmlContext::unload_document);
	godot::ClassDB::bind_method(godot::D_METHOD("get_context_info"), &RmlContext::get_context_info);

	// Auto-configuration
	godot::ClassDB::bind_method(godot::D_METHOD("get_document_path"), &RmlContext::get_document_path);
	godot::ClassDB::bind_method(godot::D_METHOD("set_document_path", "path"), &RmlContext::set_document_path);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_paths"), &RmlContext::get_font_paths);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_paths", "paths"), &RmlContext::set_font_paths);
	godot::ClassDB::bind_method(godot::D_METHOD("get_text_render_mode"), &RmlContext::get_text_render_mode);
	godot::ClassDB::bind_method(godot::D_METHOD("set_text_render_mode", "mode"), &RmlContext::set_text_render_mode);

	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "rml_context_name"), "set_rml_context_name", "get_rml_context_name");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, "dp_ratio", godot::PROPERTY_HINT_RANGE, "0.25,4.0,0.25"), "set_dp_ratio", "get_dp_ratio");

	ADD_GROUP("Auto-Configuration", "");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "document_path", godot::PROPERTY_HINT_FILE, "*.rml"), "set_document_path", "get_document_path");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_STRING_ARRAY, "font_paths"), "set_font_paths", "get_font_paths");

	ADD_GROUP("Font Settings", "");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "text_render_mode", godot::PROPERTY_HINT_ENUM, "Default,Subpixel,Oversampled,High Quality"), "set_text_render_mode", "get_text_render_mode");

});

public:
	RmlContext();
	~RmlContext() override;

	void _ready() override;
	void _process(double delta) override;
	void _draw() override;
	void _notification(int p_what);
	void _gui_input(const godot::Ref<godot::InputEvent>& event) override;

	void load_document(const godot::String& path);
	bool reload_document(const godot::String& path);
	void reload_all_documents();
	godot::Array get_loaded_documents() const;
	bool load_font_face(const godot::String& path);
	bool load_font_resource(const godot::Ref<godot::Font>& font);

	godot::String get_rml_context_name() const { return _context_name; }
	void set_rml_context_name(const godot::String& name) { _context_name = name; }

	float get_dp_ratio() const { return _dp_ratio; }
	void set_dp_ratio(float ratio);

	godot::String get_document_path() const { return _document_path; }
	void set_document_path(const godot::String& path) { _document_path = path; }
	godot::PackedStringArray get_font_paths() const { return _font_paths; }
	void set_font_paths(const godot::PackedStringArray& paths) { _font_paths = paths; }

	int get_text_render_mode() const { return _text_render_mode; }
	void set_text_render_mode(int mode);

	bool create_data_model(const godot::String& model_name);
	bool bind_data_variable(const godot::String& model_name, const godot::String& variable_name, const godot::Variant& initial_value);
	void set_data_variable(const godot::String& model_name, const godot::String& variable_name, const godot::Variant& value);
	godot::Variant get_data_variable(const godot::String& model_name, const godot::String& variable_name) const;
	bool bind_data_event(const godot::String& model_name, const godot::String& event_name, const godot::Callable& callable);
	void dirty_data_variable(const godot::String& model_name, const godot::String& variable_name);
	void dirty_all_variables(const godot::String& model_name);
	bool create_data_model_from_dict(const godot::String& model_name, const godot::Dictionary& variables);
	void update_data_model(const godot::String& model_name, const godot::Dictionary& variables);

	// Phase 3: Array data binding
	bool bind_data_array(const godot::String& model_name, const godot::String& array_name, const godot::Array& initial_array);
	void set_data_array(const godot::String& model_name, const godot::String& array_name, const godot::Array& array);
	void push_data_array_item(const godot::String& model_name, const godot::String& array_name, const godot::Variant& value);
	void remove_data_array_item(const godot::String& model_name, const godot::String& array_name, int index);
	void set_data_array_item(const godot::String& model_name, const godot::String& array_name, int index, const godot::Variant& value);
	int get_data_array_size(const godot::String& model_name, const godot::String& array_name) const;
	void clear_data_array(const godot::String& model_name, const godot::String& array_name);

	// Phase 5: Custom element instancers
	bool register_custom_element(const godot::String& tag_name, const godot::Callable& on_create,
		const godot::Callable& on_attribute_change = godot::Callable());

	// Phase 1: DOM events & element access
	bool add_event_listener(const godot::String& element_id, const godot::String& event_type,
		const godot::Callable& callable, bool in_capture_phase = false);
	void remove_event_listeners(const godot::String& element_id, const godot::String& event_type);
	godot::Ref<RmlElementHandle> get_element_by_id(const godot::String& id) const;
	bool set_element_property(const godot::String& element_id, const godot::String& property, const godot::String& value);
	void remove_element_property(const godot::String& element_id, const godot::String& property);
	void set_element_class(const godot::String& element_id, const godot::String& class_name, bool activate);
	void set_element_inner_rml(const godot::String& element_id, const godot::String& rml);
	godot::String get_element_outer_rml(const godot::String& element_id) const;
	godot::String get_element_attribute(const godot::String& element_id, const godot::String& attribute, const godot::String& default_value = "") const;
	void set_element_attribute(const godot::String& element_id, const godot::String& attribute, const godot::String& value);

	// Texture registration
	bool register_texture(const godot::String& name, const godot::Ref<godot::Texture2D>& texture);
	bool unregister_texture(const godot::String& name);

	// A4: Drag-and-drop (gd_drag interop)
	void register_drag_source(const godot::String& element_id, const godot::Callable& payload_builder = godot::Callable(), const godot::Callable& ghost_builder = godot::Callable());
	void register_drop_target(const godot::String& element_id, const godot::Callable& drop_handler = godot::Callable());
	godot::Variant _get_drag_data(const godot::Vector2& p_at_position) override;
	bool _can_drop_data(const godot::Vector2& p_at_position, const godot::Variant& p_data) const override;
	void _drop_data(const godot::Vector2& p_at_position, const godot::Variant& p_data) override;

	// Phase 8b: Dev tools & extended document management
	bool inject_stylesheet(const godot::String& rcss_string);
	bool unload_document(const godot::String& path);
	godot::Dictionary get_context_info() const;

private:
	RmlGodot::GodotRenderInterface _render_interface;
	Rml::Context* _rml_context = nullptr;
	godot::String _context_name = "default";
	float _dp_ratio = 1.0f;
	godot::String _document_path;
	godot::PackedStringArray _font_paths;
	int _text_render_mode = 0;
	bool _counted = false;

	struct LoadedDocument {
		std::string path;
		Rml::ElementDocument* document = nullptr;
	};
	std::vector<LoadedDocument> _loaded_documents;
	std::vector<godot::RID> _scissor_items;
	std::vector<godot::RID> _layer_items;
	godot::Ref<godot::CanvasItemMaterial> _premul_material;

	struct ListenerRecord {
		Rml::Element* element = nullptr;
		Rml::EventListener* listener = nullptr;
		std::string event_type;
		bool in_capture_phase = false;
	};
	std::vector<ListenerRecord> _listener_records;

	Rml::Element* _find_element(const godot::String& id) const;

	struct DataModelEntry {
		Rml::DataModelConstructor constructor;
		Rml::DataModelHandle handle;
		std::unordered_map<std::string, Rml::Variant> variables;
		std::unordered_map<std::string, godot::Callable> event_callbacks;
		std::unordered_map<std::string, Rml::Vector<Rml::String>> arrays;
	};
	std::unordered_map<std::string, DataModelEntry> _data_models;

	struct DragSourceEntry {
		std::string element_id;
		godot::Callable payload_builder;
		godot::Callable ghost_builder;
	};
	std::vector<DragSourceEntry> _drag_sources;

	struct DropTargetEntry {
		std::string element_id;
		godot::Callable drop_handler;
	};
	std::vector<DropTargetEntry> _drop_targets;

	bool _point_in_element(Rml::Element* el, float x, float y) const;
	Rml::String _build_ghost_rml(Rml::Element* el, int w, int h);
	void _create_drag_ghost(const std::string& source_element_id, const godot::Callable& ghost_builder);

	void _create_context();
	void _destroy_context();
	void _cleanup();

	void _sync_dimensions();
	void _free_scissor_items();
	void _free_layer_items();
	void _forward_mouse_event(const godot::Ref<godot::InputEvent>& event);
	void _forward_key_event(const godot::Ref<godot::InputEvent>& event);
};

} // namespace RmlGodot
