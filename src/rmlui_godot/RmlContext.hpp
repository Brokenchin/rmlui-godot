#pragma once

#include "RmlGD.hpp"
#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
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
#include "RmlDataModel.hpp"
#include "RmlDynamicData.hpp"

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

class RmlEmbedElement;

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

	// Issue #56: embed a sub-document (its own .rml + <link> RCSS + <script>) as
	// a real subtree of this context's DOM, so it shares the parent's layout
	// domain (flexbox / @media / anchoring) while keeping its own GDScript
	// instance and data feed. See RmlContextEmbed.cpp.
	godot::ClassDB::bind_method(godot::D_METHOD("mount_embed", "parent_element_id", "src", "options"), &RmlContext::mount_embed, DEFVAL(godot::Dictionary()));
	godot::ClassDB::bind_method(godot::D_METHOD("unmount_embed", "embed_id"), &RmlContext::unmount_embed);
	godot::ClassDB::bind_method(godot::D_METHOD("reload_embed", "embed_id"), &RmlContext::reload_embed);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_script", "embed_id"), &RmlContext::get_embedded_script);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_scripts", "embed_id"), &RmlContext::get_embedded_scripts);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_element", "embed_id", "inner_id"), &RmlContext::get_embedded_element);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_data", "embed_id", "model_name"), &RmlContext::get_embedded_data, DEFVAL(godot::String()));
	godot::ClassDB::bind_method(godot::D_METHOD("get_data_model_handle", "model_name"), &RmlContext::get_data_model_handle);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_ids"), &RmlContext::get_embedded_ids);
	godot::ClassDB::bind_method(godot::D_METHOD("is_embed_mounted", "embed_id"), &RmlContext::is_embed_mounted);

	// Phase 1: DOM events & element access
	godot::ClassDB::bind_method(godot::D_METHOD("add_event_listener", "element_id", "event_type", "callable", "in_capture_phase"), &RmlContext::add_event_listener, DEFVAL(false));
	godot::ClassDB::bind_method(godot::D_METHOD("remove_event_listeners", "element_id", "event_type"), &RmlContext::remove_event_listeners);
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_by_id", "id"), &RmlContext::get_element_by_id);
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_at_point", "point"), &RmlContext::get_element_at_point);
	godot::ClassDB::bind_method(godot::D_METHOD("get_focused_element"), &RmlContext::get_focused_element);
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
	godot::ClassDB::bind_method(godot::D_METHOD("get_drop_target_at_point", "point"), &RmlContext::get_drop_target_at_point);

	ADD_SIGNAL(godot::MethodInfo("rml_drag_started",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "payload")));
	ADD_SIGNAL(godot::MethodInfo("rml_drop_received",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "data")));

	// Issue #39: drag-target events — the drag-time counterpart of the hover
	// bridge. While a native drag is in progress, entering/leaving a REGISTERED
	// drop target fires these with the drag payload, so the game can highlight
	// valid targets / show a compare card without per-frame polling. rml_drag_over
	// fires only on actual cursor movement while over the target.
	godot::ClassDB::bind_method(godot::D_METHOD("get_drag_over_target"), &RmlContext::get_drag_over_target);
	ADD_SIGNAL(godot::MethodInfo("rml_drag_entered",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "data")));
	ADD_SIGNAL(godot::MethodInfo("rml_drag_over",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::DICTIONARY, "data")));
	ADD_SIGNAL(godot::MethodInfo("rml_drag_left",
		godot::PropertyInfo(godot::Variant::STRING, "element_id")));

	// Hover bridge — mirrors the drag bridge for tooltips drawn in a separate
	// overlay context (resolved by element id at event time, so it works for
	// dynamically-inserted slots that data-binding attributes can't reach).
	godot::ClassDB::bind_method(godot::D_METHOD("get_hovered_element_id"), &RmlContext::get_hovered_element_id);
	ADD_SIGNAL(godot::MethodInfo("rml_element_hovered",
		godot::PropertyInfo(godot::Variant::STRING, "element_id"),
		godot::PropertyInfo(godot::Variant::VECTOR2, "global_position")));
	ADD_SIGNAL(godot::MethodInfo("rml_element_unhovered",
		godot::PropertyInfo(godot::Variant::STRING, "element_id")));

	// Input actions & navigation (see input_actions / gamepad_navigation)
	godot::ClassDB::bind_method(godot::D_METHOD("set_input_actions", "actions"), &RmlContext::set_input_actions);
	godot::ClassDB::bind_method(godot::D_METHOD("get_input_actions"), &RmlContext::get_input_actions);
	godot::ClassDB::bind_method(godot::D_METHOD("set_gamepad_navigation", "enabled"), &RmlContext::set_gamepad_navigation);
	godot::ClassDB::bind_method(godot::D_METHOD("get_gamepad_navigation"), &RmlContext::get_gamepad_navigation);
	ADD_SIGNAL(godot::MethodInfo("rml_input_action",
		godot::PropertyInfo(godot::Variant::STRING, "action"),
		godot::PropertyInfo(godot::Variant::BOOL, "pressed")));

	// Issue #41: game-first input routing. A registerable pre-handler runs
	// BEFORE RmlUi (and the native drag) sees an event; returning true
	// consumes it. An optional per-frame tick lets time-based gestures
	// (long-press, hold-to-charge) run from a document <script> with no node.
	godot::ClassDB::bind_method(godot::D_METHOD("set_input_prehandler", "handler"), &RmlContext::set_input_prehandler, DEFVAL(godot::Callable()));
	godot::ClassDB::bind_method(godot::D_METHOD("get_input_prehandler"), &RmlContext::get_input_prehandler);
	godot::ClassDB::bind_method(godot::D_METHOD("set_input_tick", "handler"), &RmlContext::set_input_tick, DEFVAL(godot::Callable()));
	godot::ClassDB::bind_method(godot::D_METHOD("get_input_tick"), &RmlContext::get_input_tick);

	// Phase 8b: Dev tools & extended document management
	godot::ClassDB::bind_method(godot::D_METHOD("inject_stylesheet", "rcss_string"), &RmlContext::inject_stylesheet);
	godot::ClassDB::bind_method(godot::D_METHOD("unload_document", "path"), &RmlContext::unload_document);
	godot::ClassDB::bind_method(godot::D_METHOD("get_context_info"), &RmlContext::get_context_info);
	godot::ClassDB::bind_method(godot::D_METHOD("get_frame_stats"), &RmlContext::get_frame_stats);
	godot::ClassDB::bind_method(godot::D_METHOD("get_last_draw_commands"), &RmlContext::get_last_draw_commands);

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
	godot::ClassDB::bind_method(godot::D_METHOD("set_editor_scripts_enabled", "enabled"), &RmlContext::set_editor_scripts_enabled);
	godot::ClassDB::bind_method(godot::D_METHOD("is_editor_scripts_enabled"), &RmlContext::is_editor_scripts_enabled);

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
	// Per-element mouse picking (#46): with mouse_filter = STOP a Control claims
	// every event over its whole rect. Override so Godot only "sees" the context
	// where an RML element actually is — empty/transparent gaps fall through.
	bool _has_point(const godot::Vector2& point) const override;
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
	// No ADD_PROPERTY: this is a preview-panel toggle, not an authored setting —
	// kept off the inspector so scenes never persist "run scripts in editor".
	bool is_editor_scripts_enabled() const { return _editor_scripts_enabled; }
	void set_editor_scripts_enabled(bool enabled) { _editor_scripts_enabled = enabled; }
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
	bool has_data_model(const godot::String& model_name) const;
	// Upsert helpers (bind-on-first-use, then set + dirty) backing RmlDataModel.
	void dm_set_value(const godot::String& model_name, const godot::String& key, const godot::Variant& value);
	void dm_set_array(const godot::String& model_name, const godot::String& array_name, const godot::Array& array);
	void dm_push(const godot::String& model_name, const godot::String& array_name, const godot::Variant& value);

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

	// Issue #56: embedded sub-documents. mount_embed parses `src` as its own
	// document and mounts it as a child of an <embed-doc> host under
	// `parent_element_id`, so it participates in this context's layout while
	// retaining its own GDScript <script> instance(s). Returns the embed id
	// (options["id"] if given, else auto-generated) or "" on failure.
	// options: { "id": String, "model": String }.
	godot::String mount_embed(const godot::String& parent_element_id, const godot::String& src,
		const godot::Dictionary& options = godot::Dictionary());
	bool unmount_embed(const godot::String& embed_id);
	bool reload_embed(const godot::String& embed_id);
	// Mirror get_document_script(s): the embedded document's <script> instance(s),
	// so the parent can call into the embed / connect to its signals. This is the
	// explicit, collision-free parent→child data path (issue #56 req. 4).
	godot::Variant get_embedded_script(const godot::String& embed_id);
	godot::Array get_embedded_scripts(const godot::String& embed_id);
	// Resolve an element by id WITHIN a single embed's subtree. Unlike
	// get_element_by_id (which is context-global and returns the first match
	// across all embeds), this is scoped, so two embeds of the same .rml with
	// identical internal ids stay addressable independently.
	godot::Ref<RmlElementHandle> get_embedded_element(const godot::String& embed_id,
		const godot::String& inner_id) const;
	// Cached handle to an embed's data model (auto-namespaced when the embed
	// opted in via <embed-doc model="...">). model_name empty → the embed's
	// primary model. Mirror get_data_model_handle, which works for any model.
	godot::Ref<RmlDataModel> get_embedded_data(const godot::String& embed_id,
		const godot::String& model_name = godot::String());
	godot::Ref<RmlDataModel> get_data_model_handle(const godot::String& model_name);
	godot::PackedStringArray get_embedded_ids() const;
	bool is_embed_mounted(const godot::String& embed_id) const;

	// Phase 1: DOM events & element access
	bool add_event_listener(const godot::String& element_id, const godot::String& event_type,
		const godot::Callable& callable, bool in_capture_phase = false);
	void remove_event_listeners(const godot::String& element_id, const godot::String& event_type);
	godot::Ref<RmlElementHandle> get_element_by_id(const godot::String& id) const;
	// Youngest element at a context-local point (px). Crosses into embeds, so it
	// hit-tests embedded UI like any other element. Invalid handle if nothing hit.
	godot::Ref<RmlElementHandle> get_element_at_point(const godot::Vector2& point) const;
	// The element with input focus (or an invalid handle if none) — for styling
	// the focused widget, gamepad UIs, etc. Crosses into embeds.
	godot::Ref<RmlElementHandle> get_focused_element() const;
	bool set_element_property(const godot::String& element_id, const godot::String& property, const godot::String& value);
	void remove_element_property(const godot::String& element_id, const godot::String& property);
	void set_element_class(const godot::String& element_id, const godot::String& class_name, bool activate);
	void set_element_inner_rml(const godot::String& element_id, const godot::String& rml);
	godot::String get_element_outer_rml(const godot::String& element_id) const;
	godot::String get_element_attribute(const godot::String& element_id, const godot::String& attribute, const godot::String& default_value = "") const;
	void set_element_attribute(const godot::String& element_id, const godot::String& attribute, const godot::String& value);

	// --- Issue #59: embed-scoped doc-script API ---
	// Entry points called by RmlContextScope — the per-embed `rml_context`
	// injected into an embedded document's <script> blocks. Each resolves an
	// element id within `embed_id`'s subtree first, then falls back to the
	// context-global lookup. The public bound methods above are exactly the
	// embed_id == "" (root / today's) case and delegate to these. Also used for
	// <script> injection (embed_id_for_document / is_embed_namespaced).
	void set_element_inner_rml_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& rml);
	godot::Ref<RmlElementHandle> get_element_by_id_scoped(const std::string& embed_id, const godot::String& id) const;
	godot::String get_element_outer_rml_scoped(const std::string& embed_id, const godot::String& element_id) const;
	bool set_element_property_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& property, const godot::String& value);
	void remove_element_property_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& property);
	void set_element_class_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& class_name, bool activate);
	void set_element_attribute_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& attribute, const godot::String& value);
	godot::String get_element_attribute_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& attribute, const godot::String& default_value) const;
	bool add_event_listener_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& event_type, const godot::Callable& callable, bool in_capture_phase);
	void remove_event_listeners_scoped(const std::string& embed_id, const godot::String& element_id, const godot::String& event_type);
	void register_drag_source_scoped(const std::string& embed_id, const godot::String& element_id, const godot::Callable& payload_builder, const godot::Callable& ghost_builder);
	void register_drop_target_scoped(const std::string& embed_id, const godot::String& element_id, const godot::Callable& drop_handler);
	godot::String mount_embed_scoped(const std::string& embed_id, const godot::String& parent_element_id, const godot::String& src, const godot::Dictionary& options);
	// Embed introspection for the <script> injection hook (GodotScriptDocument):
	// the embed id whose mounted document is `doc` ("" if `doc` is not an embed),
	// and whether that embed opted into data-model namespacing (<embed-doc model>).
	// Non-const: while an embed is mid-mount (see _mounting_* below) this also
	// records the loading document, so the embed's on_load resolves to itself.
	std::string embed_id_for_document(Rml::ElementDocument* doc);
	bool is_embed_namespaced(const std::string& embed_id) const;

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
	// Id of the registered drop target under `point`, or "" if none. Respects
	// overflow clipping (issue #61), so it agrees with the actual drag/drop bridge
	// (_can_drop_data/_drop_data) and with get_element_at_point / the hover chain.
	godot::String get_drop_target_at_point(const godot::Vector2& point) const;
	// Issue #39: id of the registered drop target the current drag is over
	// ("" when no drag is active or none is under the cursor) — the value last
	// reported via rml_drag_entered / rml_drag_left.
	godot::String get_drag_over_target() const {
		return godot::String(_drag_over_element_id.c_str());
	}

	// Hover bridge: id of the element currently under the cursor (nearest
	// ancestor carrying an id), or "" when nothing opted-in is hovered.
	godot::String get_hovered_element_id() const;

	// Issue #41: game-first input pre-handler. `handler(event: InputEvent) ->
	// bool` runs before RmlUi (and the native drag) processes the event; a true
	// return consumes it (RmlUi + drag skip it), false/non-bool forwards as
	// usual. `tick(delta: float) -> void` is invoked every frame so time-based
	// gestures can run without a node. Pass an empty Callable to clear either.
	void set_input_prehandler(const godot::Callable& handler = godot::Callable());
	godot::Callable get_input_prehandler() const { return _input_prehandler; }
	void set_input_tick(const godot::Callable& handler = godot::Callable());
	godot::Callable get_input_tick() const { return _input_tick; }

	godot::Variant _get_drag_data(const godot::Vector2& p_at_position) override;
	bool _can_drop_data(const godot::Vector2& p_at_position, const godot::Variant& p_data) const override;
	void _drop_data(const godot::Vector2& p_at_position, const godot::Variant& p_data) override;

	// Phase 8b: Dev tools & extended document management
	bool inject_stylesheet(const godot::String& rcss_string);
	bool unload_document(const godot::String& path);
	godot::Dictionary get_context_info() const;
	// Last _draw's render-pipeline breakdown (counters + phase timings). Cheap to
	// keep hot (a few ints), so it is always on; poll from GDScript to see WHERE
	// a heavy UI frame went: RmlUi's Render walk, the slot build, or the
	// RenderingServer reconcile (slots re-applied / prims re-added / CPU clips).
	godot::Dictionary get_frame_stats() const;
	// Debug: last _draw's command stream (translation/texture/transform/scissor per
	// command) — locate an element's paint and the state it was recorded with.
	godot::Array get_last_draw_commands() const;

private:
	RmlGodot::GodotRenderInterface _render_interface;
	Rml::Context* _rml_context = nullptr;
	godot::String _context_name = "default";
	float _dp_ratio = 1.0f;
	godot::String _document_path;
	godot::PackedStringArray _font_paths;
	godot::Dictionary _editor_mock_data;
	// Editor-only gate for inline <script>/gdscript: execution. Default false:
	// in the editor user game-logic must not run (a stray loop/blocking call
	// freezes the editor — issue #29). Ignored at runtime, where scripts always
	// run. The preview panel flips this on its throwaway context when the user
	// opts in via the "Run inline scripts" checkbox.
	bool _editor_scripts_enabled = false;
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

	// Per-_draw pipeline stats (see get_frame_stats). Reset at the top of _draw;
	// incremented through the build + reconcile passes. slots_reapplied×prims is
	// the RS-call amplification to watch: a positional shift in the slot stream
	// re-applies every later slot even when its content did not change.
	struct FrameStats {
		uint32_t draw_commands = 0;
		uint32_t slots_used = 0;
		uint32_t slots_reused = 0;     // identical -> zero RS calls
		uint32_t slots_reapplied = 0;  // changed -> canvas_item_clear + re-add prims
		uint32_t slots_created = 0;
		uint32_t slots_freed = 0;
		uint32_t prims_applied = 0;    // prims (re)added via _apply_slot
		uint32_t tri_clips = 0;        // CPU polygon clips (build validation + apply)
		uint64_t render_us = 0;        // Rml::Context::Render (command generation)
		uint64_t build_us = 0;         // command stream -> slot descriptors
		uint64_t reconcile_us = 0;     // slot diff + RenderingServer calls
	};
	FrameStats _frame_stats;
	// _process-side timings (layout lives in Context::Update, not in _draw), reported
	// alongside FrameStats by get_frame_stats. A UI hitch with large update_us and small
	// render/build/reconcile is a LAYOUT spike (RmlUi re-lays-out the whole document on
	// any layout-affecting change) — the usual culprit, and invisible to _draw timings.
	uint64_t _last_update_us = 0;        // Rml::Context::Update (style + layout)
	uint64_t _last_embed_update_us = 0;  // _update_embed_layout (embed subtree reflow)
	// Per-embed layout attribution for the last _update_embed_layout pass: embed id ->
	// µs, only listing documents that did real work (>100µs). Names WHICH document a
	// layout spike belongs to (e.g. every dock view re-laying-out on each pickup).
	godot::Dictionary _last_embed_us_by_id;
	// Remaining per-frame pipeline phases (get_frame_stats): dimension sync, the
	// hover-chain diff, and input forwarding into RmlUi (ProcessMouse*/ProcessKey*,
	// accumulated across the frame's _gui_input calls, snapshotted each _process).
	uint64_t _last_sync_us = 0;
	uint64_t _last_hover_us = 0;
	uint64_t _last_input_us = 0;
	uint32_t _last_input_events = 0;
	uint64_t _input_us_accum = 0;
	uint32_t _input_events_accum = 0;

	bool _gpu_scissor = false;
	bool _render_dirty = true;
	// Issue #55: whether the context had pending animation work last _process;
	// lets the animating→idle edge queue one final settled-frame redraw.
	bool _was_animating = false;
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
	// Issue #59: resolve `id` within `embed_id`'s mounted subtree first, then fall
	// back to _find_element (context-global). embed_id "" == _find_element exactly.
	Rml::Element* _find_element_scoped(const std::string& embed_id, const godot::String& id) const;

	// --- Issue #56: embedded sub-documents ---
	// Each entry mounts an embedded document under an <embed-doc> host element in
	// the parent DOM. The host owns the embedded document through the normal child
	// mechanism; we keep raw pointers for handle resolution and lifecycle.
	struct EmbedEntry {
		std::string embed_id;
		std::string src;                          // resolved (absolute) path
		std::string model;                        // <embed-doc model="..."> → namespacing on
		std::string data_model;                   // resolved primary model name ("" = none)
		RmlEmbedElement* host = nullptr;          // <embed-doc> in the parent doc
		Rml::ElementDocument* document = nullptr; // embedded doc (host's child)
		float last_w = -1.0f;                     // last outer size (reflow tracking)
		float last_h = -1.0f;
	};
	std::unordered_map<std::string, EmbedEntry> _embeds;
	uint64_t _embed_counter = 0;
	static constexpr int k_max_embed_depth = 16;

	// #59: an embed's `load` event fires synchronously INSIDE Context::LoadDocument
	// (Context.cpp), before LoadDocument returns the document pointer — so we can't
	// register the embed before its own <script> on_load runs. These transient
	// fields announce the embed currently being mounted: embed_id_for_document()
	// resolves the loading doc's script to this embed, and _find_element_scoped()
	// resolves ids against _mounting_doc until the registry entry is in place.
	// Saved/restored around each LoadDocument for nested mounts.
	std::string _mounting_embed_id;
	Rml::ElementDocument* _mounting_doc = nullptr;
	bool _mounting_namespaced = false;

	// Mount `src` as a child of `host`; recurses into <embed-doc> authored inside
	// the embed (depth/cycle-guarded via src_chain). Does NOT call Update().
	bool _mount_embed_core(RmlEmbedElement* host, const std::string& src_raw,
		const std::string& model, const std::string& embed_id, int depth,
		std::vector<std::string>& src_chain);
	// Create an <embed-doc> host under `parent`, mount into it, run one Update.
	// Shared by mount_embed (resolves parent by id) and reload_embed.
	std::string _mount_embed_into(Rml::Element* parent, const std::string& src,
		const std::string& model, std::string embed_id);
	// Find and mount unmounted <embed-doc> elements declared inside a subtree.
	void _mount_declarative_embeds(Rml::Element* subtree_root, int depth,
		std::vector<std::string>& src_chain);
	// Resolve `src` relative to `host`'s owning document directory if not absolute.
	std::string _resolve_embed_src(Rml::Element* host, const std::string& src) const;
	std::string _next_embed_id();
	// Drop registry entries / listener records pointing into a subtree about to be
	// destroyed (unmount, or reload/unload of a top-level document).
	void _purge_embeds_in_subtree(Rml::Element* root_host);
	void _purge_listener_records_in_subtree(Rml::Element* root_host);
	// Per-frame: reflow any embed whose internal layout changed; propagate an
	// outer-size change to the parent document so siblings reposition.
	void _update_embed_layout();
	// On context resize: re-evaluate embeds' own @media queries (Context::
	// SetDimensions only covers top-level documents).
	void _redirty_embeds_media();

	struct DataModelEntry {
		Rml::DataModelConstructor constructor;
		Rml::DataModelHandle handle;
		std::unordered_map<std::string, Rml::Variant> variables;
		std::unordered_map<std::string, godot::Callable> event_callbacks;
		// Dynamic struct/scalar array backing — created lazily on first
		// bind_data_array. Owns both the custom VariableDefinitions and the
		// node trees that RmlUi points into.
		std::unique_ptr<RmlGodot::DynDataRegistry> dyn_arrays;
	};
	std::unordered_map<std::string, DataModelEntry> _data_models;

	bool _create_data_model_impl(const godot::String& model_name, bool allow_missing_variables);

	// Lookup helpers — return nullptr (optionally warning) when not found.
	DataModelEntry* _get_data_model(const godot::String& model_name, bool warn = true);
	const DataModelEntry* _get_data_model(const godot::String& model_name, bool warn = true) const;
	static RmlGodot::DynNode* _get_data_array(DataModelEntry& model,
		const godot::String& array_name, bool warn = true);

	struct DragSourceEntry {
		std::string element_id;
		std::string embed_id; // #59: scope for resolution ("" = context-global)
		godot::Callable payload_builder;
		godot::Callable ghost_builder;
	};
	std::vector<DragSourceEntry> _drag_sources;

	struct DropTargetEntry {
		std::string element_id;
		std::string embed_id; // #59: scope for resolution ("" = context-global)
		godot::Callable drop_handler;
	};
	std::vector<DropTargetEntry> _drop_targets;

	// Hover bridge: last id reported via rml_element_hovered ("" = none).
	// Polled in _process after the context Update so it tracks the live hover
	// chain (including elements streamed in via set_element_inner_rml).
	std::string _last_hovered_id;
	std::string _resolve_hovered_id() const;
	void _update_hover_tracking();

	// Issue #39: registered drop target the active drag is currently over
	// (element_id "" = none; embed_id disambiguates same-id targets across
	// embeds, #59). Two position sources feed one diff (_update_drag_over_at):
	// _can_drop_data — which the viewport calls with the exact local position on
	// every drag motion over the control — is primary; _process additionally
	// injects the polled OS-cursor position, but only when that cursor actually
	// moved, because the viewport stops calling _can_drop_data once the cursor
	// leaves the control or crosses empty space _has_point rejects — exactly
	// when the leave event must fire. The moved-only gate keeps the poll (an
	// OS-cursor read that never sees synthetic input) from fighting the
	// event-driven updates. _drag_over_last_pos throttles rml_drag_over to
	// actual cursor movement.
	std::string _drag_over_element_id;
	std::string _drag_over_embed_id;
	godot::Vector2 _drag_over_last_pos;
	godot::Vector2 _drag_poll_pos_prev;
	void _update_drag_over_tracking();
	void _update_drag_over_at(const godot::Vector2& pos);
	void _end_drag_over_tracking();

	// Issue #41: game-first input routing.
	godot::Callable _input_prehandler;   // handler(event) -> bool (true = consume)
	godot::Callable _input_tick;         // tick(delta) -> void, every frame
	// The native drag (Godot's _get_drag_data) fires only after a press plus a
	// move, so a press the pre-handler consumed must be remembered to suppress
	// the drag that the same gesture would otherwise start. Set on each mouse
	// press to that press's consume decision; checked in _get_drag_data.
	bool _prehandler_consumed_press = false;
	// Issue #47: deepest hovered element observed last _process(), used to gate
	// repaints on passive mouse-moves (a change here == the hover chain changed).
	// Compared by pointer identity only; never dereferenced.
	Rml::Element* _last_hover_element = nullptr;

	bool _point_in_element(Rml::Element* el, float x, float y) const;
	const DropTargetEntry* _drop_target_at(const godot::Vector2& point) const;
	Rml::String _build_ghost_rml(Rml::Element* el, int w, int h);
	// `el` is the drag source already resolved in its embed scope (#59) — passed
	// in so the ghost is built from the correct embed's element, not a global
	// id re-lookup. source_element_id is forwarded to the ghost_builder callback.
	void _create_drag_ghost(Rml::Element* el, const std::string& source_element_id,
		const godot::Callable& ghost_builder, const godot::Vector2& grab_offset);

	// Issue #37: drag ghost on its own CanvasLayer. Godot's native
	// set_drag_preview renders the ghost at the *source context's* stacking
	// level, so it slips under sibling widgets depending on draw order. Instead
	// the source context owns a dedicated CanvasLayer (at RmlManager's
	// configurable index) holding the ghost: it always draws above arbitrary
	// game UI. The layer follows the cursor in _process and is freed on
	// NOTIFICATION_DRAG_END (drop or cancel). _ghost_grab_offset keeps the
	// cursor at the point on the ghost where the drag was grabbed.
	godot::CanvasLayer* _ghost_layer = nullptr;
	godot::Vector2 _ghost_grab_offset;
	void _update_ghost_position();
	void _destroy_active_ghost();

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
