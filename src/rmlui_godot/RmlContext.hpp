#pragma once

#include "RmlGD.hpp"
#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include "GodotRenderInterface.hpp"
#include "RmlElementHandle.hpp"

#include <unordered_map>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/StyleSheetContainer.h>
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
	godot::ClassDB::bind_method(godot::D_METHOD("load_document_from_string", "rml_text", "alias_path"), &RmlContext::load_document_from_string, DEFVAL(godot::String("memory://document")));
	godot::ClassDB::bind_method(godot::D_METHOD("reload_document", "path"), &RmlContext::reload_document);
	godot::ClassDB::bind_method(godot::D_METHOD("reload_all_documents"), &RmlContext::reload_all_documents);
	godot::ClassDB::bind_method(godot::D_METHOD("get_loaded_documents"), &RmlContext::get_loaded_documents);
	godot::ClassDB::bind_method(godot::D_METHOD("get_document_script", "document_path"), &RmlContext::get_document_script, DEFVAL(godot::String()));
	godot::ClassDB::bind_method(godot::D_METHOD("get_document_scripts", "document_path"), &RmlContext::get_document_scripts, DEFVAL(godot::String()));
	godot::ClassDB::bind_method(godot::D_METHOD("load_font_face", "path"), &RmlContext::load_font_face);
	godot::ClassDB::bind_method(godot::D_METHOD("load_font_face_ex", "path", "family", "style", "weight", "fallback"), &RmlContext::load_font_face_ex, DEFVAL(0), DEFVAL(400), DEFVAL(false));
	godot::ClassDB::bind_method(godot::D_METHOD("load_font_resource", "font"), &RmlContext::load_font_resource);
	godot::ClassDB::bind_method(godot::D_METHOD("load_font_resource_ex", "font", "family", "weight", "fallback"), &RmlContext::load_font_resource_ex, DEFVAL(""), DEFVAL(0), DEFVAL(false));
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

	// Decorator shader registration (RCSS `decorator: shader("<name>")`)
	godot::ClassDB::bind_method(godot::D_METHOD("register_decorator_shader", "name", "shader"), &RmlContext::register_decorator_shader);
	godot::ClassDB::bind_method(godot::D_METHOD("register_decorator_material", "name", "material"), &RmlContext::register_decorator_material);
	godot::ClassDB::bind_method(godot::D_METHOD("unregister_decorator_shader", "name"), &RmlContext::unregister_decorator_shader);

	// A4: Drag-and-drop (gd_drag interop)
	godot::ClassDB::bind_method(godot::D_METHOD("register_drag_source", "element_id", "payload_builder", "ghost_builder"), &RmlContext::register_drag_source, DEFVAL(godot::Callable()), DEFVAL(godot::Callable()));
	godot::ClassDB::bind_method(godot::D_METHOD("register_drop_target", "element_id", "drop_handler"), &RmlContext::register_drop_target, DEFVAL(godot::Callable()));

	ADD_SIGNAL(godot::MethodInfo("rml_drag_started",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "payload")));
	ADD_SIGNAL(godot::MethodInfo("rml_drop_received",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "data")));

	// Input actions & navigation (see input_actions / gamepad_navigation)
	godot::ClassDB::bind_method(godot::D_METHOD("set_input_actions", "actions"), &RmlContext::set_input_actions);
	godot::ClassDB::bind_method(godot::D_METHOD("get_input_actions"), &RmlContext::get_input_actions);
	godot::ClassDB::bind_method(godot::D_METHOD("set_gamepad_navigation", "enabled"), &RmlContext::set_gamepad_navigation);
	godot::ClassDB::bind_method(godot::D_METHOD("get_gamepad_navigation"), &RmlContext::get_gamepad_navigation);
	ADD_SIGNAL(godot::MethodInfo("rml_input_action",
		godot::PropertyInfo(godot::Variant::STRING, "action"),
		godot::PropertyInfo(godot::Variant::BOOL, "pressed")));

	// Phase 8b: Dev tools & extended document management
	godot::ClassDB::bind_method(godot::D_METHOD("inject_stylesheet", "rcss_string"), &RmlContext::inject_stylesheet);
	godot::ClassDB::bind_method(godot::D_METHOD("unload_document", "path"), &RmlContext::unload_document);
	godot::ClassDB::bind_method(godot::D_METHOD("get_context_info"), &RmlContext::get_context_info);

	godot::ClassDB::bind_method(godot::D_METHOD("set_generic_family", "generic_name", "family_name"), &RmlContext::set_generic_family);
	godot::ClassDB::bind_method(godot::D_METHOD("get_generic_family", "generic_name"), &RmlContext::get_generic_family);

	// Auto-configuration
	godot::ClassDB::bind_method(godot::D_METHOD("get_document_path"), &RmlContext::get_document_path);
	godot::ClassDB::bind_method(godot::D_METHOD("set_document_path", "path"), &RmlContext::set_document_path);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_paths"), &RmlContext::get_font_paths);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_paths", "paths"), &RmlContext::set_font_paths);
	godot::ClassDB::bind_method(godot::D_METHOD("get_text_render_mode"), &RmlContext::get_text_render_mode);
	godot::ClassDB::bind_method(godot::D_METHOD("set_text_render_mode", "mode"), &RmlContext::set_text_render_mode);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_hinting"), &RmlContext::get_font_hinting);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_hinting", "hinting"), &RmlContext::set_font_hinting);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_antialiasing"), &RmlContext::get_font_antialiasing);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_antialiasing", "antialiasing"), &RmlContext::set_font_antialiasing);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_subpixel"), &RmlContext::get_font_subpixel);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_subpixel", "subpixel"), &RmlContext::set_font_subpixel);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_oversampling"), &RmlContext::get_font_oversampling);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_oversampling", "oversampling"), &RmlContext::set_font_oversampling);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_pixel_snap"), &RmlContext::get_font_pixel_snap);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_pixel_snap", "snap"), &RmlContext::set_font_pixel_snap);
	godot::ClassDB::bind_method(godot::D_METHOD("get_font_layout_mode"), &RmlContext::get_font_layout_mode);
	godot::ClassDB::bind_method(godot::D_METHOD("set_font_layout_mode", "mode"), &RmlContext::set_font_layout_mode);
	godot::ClassDB::bind_method(godot::D_METHOD("get_gpu_scissor"), &RmlContext::get_gpu_scissor);
	godot::ClassDB::bind_method(godot::D_METHOD("set_gpu_scissor", "enabled"), &RmlContext::set_gpu_scissor);
	godot::ClassDB::bind_method(godot::D_METHOD("get_use_default_rcss"), &RmlContext::get_use_default_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("set_use_default_rcss", "enabled"), &RmlContext::set_use_default_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("set_base_rcss", "rcss"), &RmlContext::set_base_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("get_base_rcss"), &RmlContext::get_base_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("append_base_rcss", "rcss"), &RmlContext::append_base_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("reset_base_rcss"), &RmlContext::reset_base_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("set_editor_mock_data", "data"), &RmlContext::set_editor_mock_data);
	godot::ClassDB::bind_method(godot::D_METHOD("get_editor_mock_data"), &RmlContext::get_editor_mock_data);

	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "rml_context_name"), "set_rml_context_name", "get_rml_context_name");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, "dp_ratio", godot::PROPERTY_HINT_RANGE, "0.25,4.0,0.25"), "set_dp_ratio", "get_dp_ratio");

	ADD_GROUP("Auto-Configuration", "");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::STRING, "document_path", godot::PROPERTY_HINT_FILE, "*.rml"), "set_document_path", "get_document_path");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_STRING_ARRAY, "font_paths"), "set_font_paths", "get_font_paths");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::PACKED_STRING_ARRAY, "input_actions"), "set_input_actions", "get_input_actions");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::BOOL, "gamepad_navigation"), "set_gamepad_navigation", "get_gamepad_navigation");
	godot::ClassDB::bind_method(godot::D_METHOD("toggle_debugger"), &RmlContext::toggle_debugger);
	godot::ClassDB::bind_method(godot::D_METHOD("get_debugger_toggle_key"), &RmlContext::get_debugger_toggle_key);
	godot::ClassDB::bind_method(godot::D_METHOD("set_debugger_toggle_key", "key"), &RmlContext::set_debugger_toggle_key);
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "debugger_toggle_key"), "set_debugger_toggle_key", "get_debugger_toggle_key");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::BOOL, "use_default_rcss"), "set_use_default_rcss", "get_use_default_rcss");

	ADD_GROUP("Text Rendering", "");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "text_render_mode", godot::PROPERTY_HINT_ENUM, "Default,SubPixel Offset,Godot Native,RmlUI Native"), "set_text_render_mode", "get_text_render_mode");
	godot::ClassDB::bind_method(godot::D_METHOD("get_text_filtering_mode"), &RmlContext::get_text_filtering_mode);
	godot::ClassDB::bind_method(godot::D_METHOD("set_text_filtering_mode", "mode"), &RmlContext::set_text_filtering_mode);
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "text_filtering_mode", godot::PROPERTY_HINT_ENUM, "Nearest,Linear"), "set_text_filtering_mode", "get_text_filtering_mode");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::BOOL, "font_pixel_snap"), "set_font_pixel_snap", "get_font_pixel_snap");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "font_layout_mode", godot::PROPERTY_HINT_ENUM, "Manual,Integer Advance,Shaped"), "set_font_layout_mode", "get_font_layout_mode");

	ADD_GROUP("Font Face Overrides (load_font_face only)", "");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "font_hinting", godot::PROPERTY_HINT_ENUM, "None,Light,Normal"), "set_font_hinting", "get_font_hinting");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "font_antialiasing", godot::PROPERTY_HINT_ENUM, "None,Gray,LCD Subpixel"), "set_font_antialiasing", "get_font_antialiasing");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::INT, "font_subpixel", godot::PROPERTY_HINT_ENUM, "Disabled,Auto,One Half,One Quarter"), "set_font_subpixel", "get_font_subpixel");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::FLOAT, "font_oversampling", godot::PROPERTY_HINT_RANGE, "0.0,4.0,0.5"), "set_font_oversampling", "get_font_oversampling");

	ADD_GROUP("Scissor Clipping", "");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::BOOL, "gpu_scissor"), "set_gpu_scissor", "get_gpu_scissor");

	ADD_GROUP("Editor Preview", "");
	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::DICTIONARY, "editor_mock_data"), "set_editor_mock_data", "get_editor_mock_data");

});

public:
	RmlContext();
	~RmlContext() override;

	void _ready() override;
	void _process(double delta) override;
	void _draw() override;
	void _notification(int p_what);
	void _gui_input(const godot::Ref<godot::InputEvent>& event) override;
	void _unhandled_input(const godot::Ref<godot::InputEvent>& event) override;
	godot::PackedStringArray _get_configuration_warnings() const override;

	godot::PackedStringArray get_input_actions() const { return _input_actions; }
	void set_input_actions(const godot::PackedStringArray& actions);
	bool get_gamepad_navigation() const { return _gamepad_navigation; }
	void set_gamepad_navigation(bool enabled);
	void toggle_debugger();
	// Request a redraw on the next frame. Called by every state-mutating API
	// (DOM, data binding, element handles) — the dirty-flag render gate
	// otherwise skips frames and external mutations never show until input
	// or an animation forces an update.
	void mark_render_dirty() { _render_dirty = true; }
	int64_t get_debugger_toggle_key() const { return _debugger_toggle_key; }
	void set_debugger_toggle_key(int64_t key) {
		_debugger_toggle_key = key;
		_update_unhandled_input_processing();
	}

private:
	void _update_unhandled_input_processing();
	bool _process_navigation_input(const godot::Ref<godot::InputEvent>& event);
	void _apply_editor_mock_data();

public:

	void load_document(const godot::String& path);
	bool load_document_from_string(const godot::String& rml_text, const godot::String& alias_path = "memory://document");
	bool reload_document(const godot::String& path);
	void reload_all_documents();
	godot::Array get_loaded_documents() const;
	godot::Variant get_document_script(const godot::String& document_path = "");
	godot::Array get_document_scripts(const godot::String& document_path = "");
	bool load_font_face(const godot::String& path);
	bool load_font_face_ex(const godot::String& path, const godot::String& family, int style = 0, int weight = 400, bool fallback = false);
	bool load_font_resource(const godot::Ref<godot::Font>& font);
	bool load_font_resource_ex(const godot::Ref<godot::Font>& font, const godot::String& family = "", int weight = 0, bool fallback = false);

	godot::String get_rml_context_name() const { return _context_name; }
	void set_rml_context_name(const godot::String& name) { _context_name = name; }

	float get_dp_ratio() const { return _dp_ratio; }
	void set_dp_ratio(float ratio);

	godot::String get_document_path() const { return _document_path; }
	void set_document_path(const godot::String& path);
	godot::Dictionary get_editor_mock_data() const { return _editor_mock_data; }
	void set_editor_mock_data(const godot::Dictionary& data) { _editor_mock_data = data; }
	godot::PackedStringArray get_font_paths() const { return _font_paths; }
	void set_font_paths(const godot::PackedStringArray& paths);

	int get_text_render_mode() const { return _text_render_mode; }
	void set_text_render_mode(int mode);

	// Texture filter for TEXT GLYPHS only (Nearest=0 default, Linear=1).
	// Deliberately independent of the node's texture_filter and the project
	// default: glyph quads currently sample off the texel grid under linear
	// (see the SUBPIX_OFFSET alignment follow-up), so text defaults to
	// nearest — crisp like Godot's own — while images keep normal filtering.
	int get_text_filtering_mode() const { return _text_filtering_mode; }
	void set_text_filtering_mode(int mode) {
		_text_filtering_mode = mode;
		_render_dirty = true;
	}

	int get_font_hinting() const { return _font_hinting; }
	void set_font_hinting(int hinting);
	int get_font_antialiasing() const { return _font_antialiasing; }
	void set_font_antialiasing(int antialiasing);
	int get_font_subpixel() const { return _font_subpixel; }
	void set_font_subpixel(int subpixel);
	float get_font_oversampling() const { return _font_oversampling; }
	void set_font_oversampling(float oversampling);
	bool get_font_pixel_snap() const { return _font_pixel_snap; }
	void set_font_pixel_snap(bool snap);
	int get_font_layout_mode() const { return _font_layout_mode; }
	void set_font_layout_mode(int mode);

	bool get_gpu_scissor() const { return _gpu_scissor; }
	void set_gpu_scissor(bool enabled);

	bool get_use_default_rcss() const { return _use_default_rcss; }
	void set_use_default_rcss(bool enabled) { _use_default_rcss = enabled; }

	void set_base_rcss(const godot::String& rcss);
	godot::String get_base_rcss() const;
	void append_base_rcss(const godot::String& rcss);
	void reset_base_rcss();

	void set_generic_family(const godot::String& generic_name, const godot::String& family_name);
	godot::String get_generic_family(const godot::String& generic_name) const;

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

	// Decorator shader registration
	bool register_decorator_shader(const godot::String& name, const godot::Ref<godot::Shader>& shader);
	bool register_decorator_material(const godot::String& name, const godot::Ref<godot::ShaderMaterial>& material);
	bool unregister_decorator_shader(const godot::String& name);

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
	godot::Dictionary _editor_mock_data;
	godot::PackedStringArray _input_actions;
	bool _gamepad_navigation = false;
	// Key toggling the RmlUi debugger overlay (godot::Key value, 0 = disabled).
	// Default F10 — with the 4.5 embedded game window F8 is the editor's
	// Stop shortcut and F9 its Pause, both fatal/disruptive.
	int64_t _debugger_toggle_key = static_cast<int64_t>(godot::KEY_F10);
	int _text_render_mode = 0;    // DEFAULT (resolves to SUBPIX_OFFSET)
	int _text_filtering_mode = 0; // 0 = Nearest (crisp), 1 = Linear
	// Granular font tuning (defaults match Godot's FontFile import + Label).
	int _font_hinting = 1;        // Light
	int _font_antialiasing = 1;   // Gray
	int _font_subpixel = 1;       // Auto
	float _font_oversampling = 0.0f;
	bool _font_pixel_snap = true;
	int _font_layout_mode = 0;    // Manual
	bool _counted = false;

	struct LoadedDocument {
		std::string path;
		Rml::ElementDocument* document = nullptr;
	};
	std::vector<LoadedDocument> _loaded_documents;
	godot::Ref<godot::Material> _active_material;

	// --- Persistent canvas-item slot model (redraw coalescing, issue #14) ---
	// Each frame _draw() builds a flat, ordered list of canvas-item "slots"
	// (the root item, layer/clip-mask group items, and batched geometry "runs")
	// as plain descriptors — no RenderingServer calls. _reconcile_slots() then
	// diffs that list against the previous frame's slots by position: an
	// identical slot keeps its RID untouched (zero RS work), a changed slot
	// reuses the same RID via canvas_item_clear + re-add, and trailing slots no
	// longer produced are freed. RmlUi already caches compiled geometry per
	// element (unchanged elements keep a stable, monotonically-allocated
	// CompiledGeometryHandle), so a localized change — e.g. one cell's :hover —
	// touches only the handful of slots that actually differ instead of tearing
	// down and rebuilding the whole tree every frame.
	struct SlotPrim {
		enum Kind : uint8_t { MESH, TRI_ARRAY };
		Kind kind = MESH;
		// Identity (compared to decide reuse). Geometry handles are never
		// recycled, so an unchanged element keeps the same handle and a
		// recompiled one gets a fresh handle — exact change detection.
		uintptr_t geo_handle = 0;
		uintptr_t tex_handle = 0;
		godot::Transform2D xform;
		godot::Color modulate{1, 1, 1, 1};
		godot::Rect2 clip_rect;      // TRI_ARRAY: CPU-clip rect (re-clipped on apply)
		// Resolved resources used to (re)build the draw on apply.
		godot::RID mesh_rid;
		godot::RID tex_rid;
	};

	struct SlotDesc {
		enum Kind : uint8_t { ROOT, GROUP, RUN };
		Kind kind = RUN;
		int parent = -1;             // slot index of parent; -1 => get_canvas_item()
		godot::RID material;
		int filter = 0;              // RenderingServer::CanvasItemTextureFilter
		int draw_index = -1;         // -1 => leave default (root/groups)
		int group_mode = -1;         // -1 => don't set; else CanvasGroupMode
		bool modulate_set = false;
		godot::Color modulate{1, 1, 1, 1};
		bool set_scissor_param = false;
		godot::Vector4 scissor_param;
		std::vector<SlotPrim> prims; // drawn directly into this item
	};

	struct Slot {
		SlotDesc desc;
		godot::RID rid;
		godot::RID parent_rid;
	};
	// Two ping-ponged slot buffers: one holds the previous frame, the other is
	// (re)built this frame. Reusing the buffers — and each Slot's prims vector
	// (cleared, not destroyed) — means steady-state frames allocate nothing,
	// which is what keeps the all-mutated worst case from regressing.
	std::vector<Slot> _slots_buf[2];
	uint8_t _slots_cur = 0;    // buffer index holding the previous frame
	size_t _slots_count = 0;   // valid slot count in the previous buffer

	bool _gpu_scissor = false;
	bool _render_dirty = true;
	int _last_font_version = -1;
	bool _use_default_rcss = true;
	std::string _local_base_rcss;
	bool _has_local_base_rcss = false;
	godot::Ref<godot::ShaderMaterial> _scissor_material;
	void _ensure_scissor_material();
	void _apply_base_stylesheet(Rml::ElementDocument* doc);
	Rml::SharedPtr<Rml::StyleSheetContainer> _get_effective_base_sheet();

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

	// RmlUi's data type register is per-context — must not be shared globally.
	bool _array_type_registered = false;

	// Lookup helpers — return nullptr (optionally warning) when not found.
	DataModelEntry* _get_data_model(const godot::String& model_name, bool warn = true);
	const DataModelEntry* _get_data_model(const godot::String& model_name, bool warn = true) const;
	static Rml::Vector<Rml::String>* _get_data_array(DataModelEntry& model,
		const godot::String& array_name, bool warn = true);
	static const Rml::Vector<Rml::String>* _get_data_array(const DataModelEntry& model,
		const godot::String& array_name, bool warn = true);

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
	// Diff the freshly-built slot buffer (the first `used` entries of the
	// non-current buffer) against the previous frame and emit the minimal set
	// of RenderingServer calls; then flips _slots_cur.
	void _reconcile_slots(size_t used);
	static bool _prim_equal(const SlotPrim& a, const SlotPrim& b);
	static bool _desc_equal(const SlotDesc& a, const SlotDesc& b);
	// (Re)apply a slot's full state to its canvas item: parent, material,
	// filter, draw index, group mode, modulate, scissor uniform, and prims.
	void _apply_slot(int slot_index, const SlotDesc& desc, const godot::RID& parent_rid,
		const godot::RID& canvas_item);
	void _free_all_slots();
	void _forward_mouse_event(const godot::Ref<godot::InputEvent>& event);
	void _forward_key_event(const godot::Ref<godot::InputEvent>& event);
};

} // namespace RmlGodot
