#pragma once

#include "RmlGD.hpp"
#include "RmlElementHandle.hpp"
#include "RmlDataModel.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <string>

namespace RmlGodot {

class RmlContext;

/// Issue #59: the per-script `rml_context` injected into an EMBEDDED document's
/// inline <script> blocks (root documents still receive the raw RmlContext node,
/// unchanged). A doc-script widget addresses elements by id —
/// `rml_context.set_element_inner_rml("grid", ...)`,
/// `rml_context.register_drag_source("slot_0", ...)`, etc. — and standalone those
/// ids are unambiguous. Mounted twice in one context (two <embed-doc>s of the same
/// .rml), the ids collide: the context-global lookup returns the first match, so
/// both widgets would drive the SAME embed.
///
/// This scope object closes that gap WITHOUT touching the widget's source: it
/// carries the embed it belongs to and resolves every id-taking call within that
/// embed's own subtree first (then falls back to the context-global lookup, so a
/// shared/parent id still works). Data-model names are auto-namespaced for an
/// embed that opted in via <embed-doc model="...">, mirroring the injected `data`
/// handle and get_embedded_data. So a widget that runs standalone runs embedded —
/// byte-for-byte — with independent element resolution and data per instance.
///
/// Everything that is genuinely context-global (textures, decorators, hit-testing,
/// focus) forwards to the node unchanged. get_context_node() is the escape hatch
/// for the rare widget that needs the actual node (e.g. to connect to a node
/// signal).
///
/// Lifetime: weak reference to the owning RmlContext (validated each call, like
/// RmlDataModel) — methods no-op once the context is freed; check is_valid().
class RM_GD_CLASS(RmlContextScope, godot::RefCounted, {

	godot::ClassDB::bind_method(godot::D_METHOD("is_valid"), &RmlContextScope::is_valid);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embed_id"), &RmlContextScope::get_embed_id);
	godot::ClassDB::bind_method(godot::D_METHOD("get_context_node"), &RmlContextScope::get_context_node);

	// Element access (resolved within this embed's subtree first).
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_by_id", "id"), &RmlContextScope::get_element_by_id);
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_inner_rml", "element_id", "rml"), &RmlContextScope::set_element_inner_rml);
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_outer_rml", "element_id"), &RmlContextScope::get_element_outer_rml);
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_property", "element_id", "property", "value"), &RmlContextScope::set_element_property);
	godot::ClassDB::bind_method(godot::D_METHOD("remove_element_property", "element_id", "property"), &RmlContextScope::remove_element_property);
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_class", "element_id", "class_name", "activate"), &RmlContextScope::set_element_class);
	godot::ClassDB::bind_method(godot::D_METHOD("set_element_attribute", "element_id", "attribute", "value"), &RmlContextScope::set_element_attribute);
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_attribute", "element_id", "attribute", "default_value"), &RmlContextScope::get_element_attribute, DEFVAL(""));
	godot::ClassDB::bind_method(godot::D_METHOD("add_event_listener", "element_id", "event_type", "callable", "in_capture_phase"), &RmlContextScope::add_event_listener, DEFVAL(false));
	godot::ClassDB::bind_method(godot::D_METHOD("remove_event_listeners", "element_id", "event_type"), &RmlContextScope::remove_event_listeners);

	// Drag & drop (registry keyed by this embed's scope, so two embeds of the
	// same widget register the same ids independently).
	godot::ClassDB::bind_method(godot::D_METHOD("register_drag_source", "element_id", "payload_builder", "ghost_builder"), &RmlContextScope::register_drag_source, DEFVAL(godot::Callable()), DEFVAL(godot::Callable()));
	godot::ClassDB::bind_method(godot::D_METHOD("register_drop_target", "element_id", "drop_handler"), &RmlContextScope::register_drop_target, DEFVAL(godot::Callable()));

	// Nested embeds: parent_element_id resolves within this embed's subtree.
	godot::ClassDB::bind_method(godot::D_METHOD("mount_embed", "parent_element_id", "src", "options"), &RmlContextScope::mount_embed, DEFVAL(godot::Dictionary()));
	godot::ClassDB::bind_method(godot::D_METHOD("unmount_embed", "embed_id"), &RmlContextScope::unmount_embed);
	godot::ClassDB::bind_method(godot::D_METHOD("reload_embed", "embed_id"), &RmlContextScope::reload_embed);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_element", "embed_id", "inner_id"), &RmlContextScope::get_embedded_element);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_script", "embed_id"), &RmlContextScope::get_embedded_script);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_scripts", "embed_id"), &RmlContextScope::get_embedded_scripts);
	godot::ClassDB::bind_method(godot::D_METHOD("is_embed_mounted", "embed_id"), &RmlContextScope::is_embed_mounted);
	godot::ClassDB::bind_method(godot::D_METHOD("get_embedded_ids"), &RmlContextScope::get_embedded_ids);

	// Data binding (model name auto-namespaced when this embed opted in).
	godot::ClassDB::bind_method(godot::D_METHOD("create_data_model", "model_name"), &RmlContextScope::create_data_model);
	godot::ClassDB::bind_method(godot::D_METHOD("has_data_model", "model_name"), &RmlContextScope::has_data_model);
	godot::ClassDB::bind_method(godot::D_METHOD("bind_data_variable", "model_name", "variable_name", "initial_value"), &RmlContextScope::bind_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("set_data_variable", "model_name", "variable_name", "value"), &RmlContextScope::set_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("get_data_variable", "model_name", "variable_name"), &RmlContextScope::get_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("bind_data_event", "model_name", "event_name", "callable"), &RmlContextScope::bind_data_event);
	godot::ClassDB::bind_method(godot::D_METHOD("dirty_data_variable", "model_name", "variable_name"), &RmlContextScope::dirty_data_variable);
	godot::ClassDB::bind_method(godot::D_METHOD("dirty_all_variables", "model_name"), &RmlContextScope::dirty_all_variables);
	godot::ClassDB::bind_method(godot::D_METHOD("create_data_model_from_dict", "model_name", "variables"), &RmlContextScope::create_data_model_from_dict);
	godot::ClassDB::bind_method(godot::D_METHOD("update_data_model", "model_name", "variables"), &RmlContextScope::update_data_model);
	godot::ClassDB::bind_method(godot::D_METHOD("bind_data_array", "model_name", "array_name", "initial_array"), &RmlContextScope::bind_data_array);
	godot::ClassDB::bind_method(godot::D_METHOD("set_data_array", "model_name", "array_name", "array"), &RmlContextScope::set_data_array);
	godot::ClassDB::bind_method(godot::D_METHOD("push_data_array_item", "model_name", "array_name", "value"), &RmlContextScope::push_data_array_item);
	godot::ClassDB::bind_method(godot::D_METHOD("remove_data_array_item", "model_name", "array_name", "index"), &RmlContextScope::remove_data_array_item);
	godot::ClassDB::bind_method(godot::D_METHOD("set_data_array_item", "model_name", "array_name", "index", "value"), &RmlContextScope::set_data_array_item);
	godot::ClassDB::bind_method(godot::D_METHOD("get_data_array_size", "model_name", "array_name"), &RmlContextScope::get_data_array_size);
	godot::ClassDB::bind_method(godot::D_METHOD("clear_data_array", "model_name", "array_name"), &RmlContextScope::clear_data_array);
	godot::ClassDB::bind_method(godot::D_METHOD("get_data_model_handle", "model_name"), &RmlContextScope::get_data_model_handle);

	// Context-global pass-throughs (no id, no model — forwarded unchanged).
	godot::ClassDB::bind_method(godot::D_METHOD("get_element_at_point", "point"), &RmlContextScope::get_element_at_point);
	godot::ClassDB::bind_method(godot::D_METHOD("get_focused_element"), &RmlContextScope::get_focused_element);
	godot::ClassDB::bind_method(godot::D_METHOD("get_hovered_element_id"), &RmlContextScope::get_hovered_element_id);
	godot::ClassDB::bind_method(godot::D_METHOD("register_texture", "name", "texture"), &RmlContextScope::register_texture);
	godot::ClassDB::bind_method(godot::D_METHOD("unregister_texture", "name"), &RmlContextScope::unregister_texture);
	godot::ClassDB::bind_method(godot::D_METHOD("register_decorator_shader", "name", "shader"), &RmlContextScope::register_decorator_shader);
	godot::ClassDB::bind_method(godot::D_METHOD("register_decorator_material", "name", "material"), &RmlContextScope::register_decorator_material);
	godot::ClassDB::bind_method(godot::D_METHOD("unregister_decorator_shader", "name"), &RmlContextScope::unregister_decorator_shader);
	godot::ClassDB::bind_method(godot::D_METHOD("inject_stylesheet", "rcss_string"), &RmlContextScope::inject_stylesheet);

});

public:
	RmlContextScope() = default;
	~RmlContextScope() override = default;

	/// Internal: bind this scope to (context, embed). `namespaced` mirrors the
	/// embed's <embed-doc model="..."> opt-in (data-model names get the
	/// `embed_id::` prefix). Called by GodotScriptDocument at injection time.
	void setup(RmlContext* context, const std::string& embed_id, bool namespaced);

	bool is_valid() const;
	godot::String get_embed_id() const { return godot::String(_embed_id.c_str()); }
	RmlContext* get_context_node() const;

	// Element access (embed-scoped).
	godot::Ref<RmlElementHandle> get_element_by_id(const godot::String& id) const;
	void set_element_inner_rml(const godot::String& element_id, const godot::String& rml);
	godot::String get_element_outer_rml(const godot::String& element_id) const;
	bool set_element_property(const godot::String& element_id, const godot::String& property, const godot::String& value);
	void remove_element_property(const godot::String& element_id, const godot::String& property);
	void set_element_class(const godot::String& element_id, const godot::String& class_name, bool activate);
	void set_element_attribute(const godot::String& element_id, const godot::String& attribute, const godot::String& value);
	godot::String get_element_attribute(const godot::String& element_id, const godot::String& attribute, const godot::String& default_value = "") const;
	bool add_event_listener(const godot::String& element_id, const godot::String& event_type, const godot::Callable& callable, bool in_capture_phase = false);
	void remove_event_listeners(const godot::String& element_id, const godot::String& event_type);

	// Drag & drop (embed-scoped).
	void register_drag_source(const godot::String& element_id, const godot::Callable& payload_builder = godot::Callable(), const godot::Callable& ghost_builder = godot::Callable());
	void register_drop_target(const godot::String& element_id, const godot::Callable& drop_handler = godot::Callable());

	// Embeds.
	godot::String mount_embed(const godot::String& parent_element_id, const godot::String& src, const godot::Dictionary& options = godot::Dictionary());
	bool unmount_embed(const godot::String& embed_id);
	bool reload_embed(const godot::String& embed_id);
	godot::Ref<RmlElementHandle> get_embedded_element(const godot::String& embed_id, const godot::String& inner_id) const;
	godot::Variant get_embedded_script(const godot::String& embed_id);
	godot::Array get_embedded_scripts(const godot::String& embed_id);
	bool is_embed_mounted(const godot::String& embed_id) const;
	godot::PackedStringArray get_embedded_ids() const;

	// Data binding (model name namespaced when this embed opted in).
	bool create_data_model(const godot::String& model_name);
	bool has_data_model(const godot::String& model_name) const;
	bool bind_data_variable(const godot::String& model_name, const godot::String& variable_name, const godot::Variant& initial_value);
	void set_data_variable(const godot::String& model_name, const godot::String& variable_name, const godot::Variant& value);
	godot::Variant get_data_variable(const godot::String& model_name, const godot::String& variable_name) const;
	bool bind_data_event(const godot::String& model_name, const godot::String& event_name, const godot::Callable& callable);
	void dirty_data_variable(const godot::String& model_name, const godot::String& variable_name);
	void dirty_all_variables(const godot::String& model_name);
	bool create_data_model_from_dict(const godot::String& model_name, const godot::Dictionary& variables);
	void update_data_model(const godot::String& model_name, const godot::Dictionary& variables);
	bool bind_data_array(const godot::String& model_name, const godot::String& array_name, const godot::Array& initial_array);
	void set_data_array(const godot::String& model_name, const godot::String& array_name, const godot::Array& array);
	void push_data_array_item(const godot::String& model_name, const godot::String& array_name, const godot::Variant& value);
	void remove_data_array_item(const godot::String& model_name, const godot::String& array_name, int index);
	void set_data_array_item(const godot::String& model_name, const godot::String& array_name, int index, const godot::Variant& value);
	int get_data_array_size(const godot::String& model_name, const godot::String& array_name) const;
	void clear_data_array(const godot::String& model_name, const godot::String& array_name);
	godot::Ref<RmlDataModel> get_data_model_handle(const godot::String& model_name);

	// Context-global pass-throughs.
	godot::Ref<RmlElementHandle> get_element_at_point(const godot::Vector2& point) const;
	godot::Ref<RmlElementHandle> get_focused_element() const;
	godot::String get_hovered_element_id() const;
	bool register_texture(const godot::String& name, const godot::Ref<godot::Texture2D>& texture);
	bool unregister_texture(const godot::String& name);
	bool register_decorator_shader(const godot::String& name, const godot::Ref<godot::Shader>& shader);
	bool register_decorator_material(const godot::String& name, const godot::Ref<godot::ShaderMaterial>& material);
	bool unregister_decorator_shader(const godot::String& name);
	bool inject_stylesheet(const godot::String& rcss_string);

private:
	// Weak handle to the owning context (RmlContext is a Node, not RefCounted):
	// resolved through ObjectDB each call so a freed context returns null.
	uint64_t _context_id = 0;
	std::string _embed_id;
	bool _namespaced = false;

	RmlContext* _resolve() const;
	// Apply this embed's namespace to a data-model name (no-op unless opted in).
	godot::String _model(const godot::String& model_name) const;
};

} // namespace RmlGodot
