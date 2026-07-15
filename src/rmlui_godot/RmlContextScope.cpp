#include "RmlContextScope.hpp"
#include "RmlContext.hpp"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/object.hpp>

namespace RmlGodot {

void RmlContextScope::setup(RmlContext* context, const std::string& embed_id, bool namespaced) {
	_context_id = (context != nullptr) ? context->get_instance_id() : 0;
	_embed_id = embed_id;
	_namespaced = namespaced;
}

RmlContext* RmlContextScope::_resolve() const {
	if (_context_id == 0) return nullptr;
	godot::Object* obj = godot::ObjectDB::get_instance(_context_id);
	return godot::Object::cast_to<RmlContext>(obj);
}

bool RmlContextScope::is_valid() const {
	return _resolve() != nullptr && !_embed_id.empty();
}

RmlContext* RmlContextScope::get_context_node() const {
	return _resolve();
}

godot::String RmlContextScope::_model(const godot::String& model_name) const {
	// Namespacing mirrors how the embed's data-model attributes were rewritten at
	// mount (<embed-doc model="...">): authored name "X" → "<embed_id>::X". A
	// non-namespaced embed binds context models by their authored names (today's
	// behavior), so the name passes through unchanged.
	if (!_namespaced) return model_name;
	return godot::String((_embed_id + "::").c_str()) + model_name;
}

// --- Element access (embed-scoped) ---

godot::Ref<RmlElementHandle> RmlContextScope::get_element_by_id(const godot::String& id) const {
	if (RmlContext* ctx = _resolve()) return ctx->get_element_by_id_scoped(_embed_id, id);
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	return handle;
}

void RmlContextScope::set_element_inner_rml(const godot::String& element_id, const godot::String& rml) {
	if (RmlContext* ctx = _resolve()) ctx->set_element_inner_rml_scoped(_embed_id, element_id, rml);
}

godot::String RmlContextScope::get_element_outer_rml(const godot::String& element_id) const {
	if (RmlContext* ctx = _resolve()) return ctx->get_element_outer_rml_scoped(_embed_id, element_id);
	return {};
}

bool RmlContextScope::set_element_property(const godot::String& element_id,
		const godot::String& property, const godot::String& value) {
	if (RmlContext* ctx = _resolve()) return ctx->set_element_property_scoped(_embed_id, element_id, property, value);
	return false;
}

void RmlContextScope::remove_element_property(const godot::String& element_id, const godot::String& property) {
	if (RmlContext* ctx = _resolve()) ctx->remove_element_property_scoped(_embed_id, element_id, property);
}

void RmlContextScope::set_element_class(const godot::String& element_id,
		const godot::String& class_name, bool activate) {
	if (RmlContext* ctx = _resolve()) ctx->set_element_class_scoped(_embed_id, element_id, class_name, activate);
}

void RmlContextScope::set_element_attribute(const godot::String& element_id,
		const godot::String& attribute, const godot::String& value) {
	if (RmlContext* ctx = _resolve()) ctx->set_element_attribute_scoped(_embed_id, element_id, attribute, value);
}

godot::String RmlContextScope::get_element_attribute(const godot::String& element_id,
		const godot::String& attribute, const godot::String& default_value) const {
	if (RmlContext* ctx = _resolve()) return ctx->get_element_attribute_scoped(_embed_id, element_id, attribute, default_value);
	return default_value;
}

bool RmlContextScope::add_event_listener(const godot::String& element_id, const godot::String& event_type,
		const godot::Callable& callable, bool in_capture_phase) {
	if (RmlContext* ctx = _resolve()) return ctx->add_event_listener_scoped(_embed_id, element_id, event_type, callable, in_capture_phase);
	return false;
}

void RmlContextScope::remove_event_listeners(const godot::String& element_id, const godot::String& event_type) {
	if (RmlContext* ctx = _resolve()) ctx->remove_event_listeners_scoped(_embed_id, element_id, event_type);
}

// --- Drag & drop (embed-scoped) ---

void RmlContextScope::register_drag_source(const godot::String& element_id,
		const godot::Callable& payload_builder, const godot::Callable& ghost_builder) {
	if (RmlContext* ctx = _resolve()) ctx->register_drag_source_scoped(_embed_id, element_id, payload_builder, ghost_builder);
}

void RmlContextScope::register_drop_target(const godot::String& element_id, const godot::Callable& drop_handler) {
	if (RmlContext* ctx = _resolve()) ctx->register_drop_target_scoped(_embed_id, element_id, drop_handler);
}

// --- Embeds ---

godot::String RmlContextScope::mount_embed(const godot::String& parent_element_id,
		const godot::String& src, const godot::Dictionary& options) {
	if (RmlContext* ctx = _resolve()) return ctx->mount_embed_scoped(_embed_id, parent_element_id, src, options);
	return {};
}

bool RmlContextScope::unmount_embed(const godot::String& embed_id) {
	if (RmlContext* ctx = _resolve()) return ctx->unmount_embed(embed_id);
	return false;
}

bool RmlContextScope::reload_embed(const godot::String& embed_id) {
	if (RmlContext* ctx = _resolve()) return ctx->reload_embed(embed_id);
	return false;
}

godot::Ref<RmlElementHandle> RmlContextScope::get_embedded_element(const godot::String& embed_id,
		const godot::String& inner_id) const {
	if (RmlContext* ctx = _resolve()) return ctx->get_embedded_element(embed_id, inner_id);
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	return handle;
}

godot::Variant RmlContextScope::get_embedded_script(const godot::String& embed_id) {
	if (RmlContext* ctx = _resolve()) return ctx->get_embedded_script(embed_id);
	return {};
}

godot::Array RmlContextScope::get_embedded_scripts(const godot::String& embed_id) {
	if (RmlContext* ctx = _resolve()) return ctx->get_embedded_scripts(embed_id);
	return {};
}

bool RmlContextScope::is_embed_mounted(const godot::String& embed_id) const {
	if (RmlContext* ctx = _resolve()) return ctx->is_embed_mounted(embed_id);
	return false;
}

godot::PackedStringArray RmlContextScope::get_embedded_ids() const {
	if (RmlContext* ctx = _resolve()) return ctx->get_embedded_ids();
	return {};
}

// --- Data binding (model namespaced when this embed opted in) ---

bool RmlContextScope::create_data_model(const godot::String& model_name) {
	if (RmlContext* ctx = _resolve()) return ctx->create_data_model(_model(model_name));
	return false;
}

bool RmlContextScope::has_data_model(const godot::String& model_name) const {
	if (RmlContext* ctx = _resolve()) return ctx->has_data_model(_model(model_name));
	return false;
}

bool RmlContextScope::bind_data_variable(const godot::String& model_name,
		const godot::String& variable_name, const godot::Variant& initial_value) {
	if (RmlContext* ctx = _resolve()) return ctx->bind_data_variable(_model(model_name), variable_name, initial_value);
	return false;
}

void RmlContextScope::set_data_variable(const godot::String& model_name,
		const godot::String& variable_name, const godot::Variant& value) {
	if (RmlContext* ctx = _resolve()) ctx->set_data_variable(_model(model_name), variable_name, value);
}

godot::Variant RmlContextScope::get_data_variable(const godot::String& model_name,
		const godot::String& variable_name) const {
	if (RmlContext* ctx = _resolve()) return ctx->get_data_variable(_model(model_name), variable_name);
	return {};
}

bool RmlContextScope::bind_data_event(const godot::String& model_name,
		const godot::String& event_name, const godot::Callable& callable) {
	if (RmlContext* ctx = _resolve()) return ctx->bind_data_event(_model(model_name), event_name, callable);
	return false;
}

void RmlContextScope::dirty_data_variable(const godot::String& model_name, const godot::String& variable_name) {
	if (RmlContext* ctx = _resolve()) ctx->dirty_data_variable(_model(model_name), variable_name);
}

void RmlContextScope::dirty_all_variables(const godot::String& model_name) {
	if (RmlContext* ctx = _resolve()) ctx->dirty_all_variables(_model(model_name));
}

bool RmlContextScope::create_data_model_from_dict(const godot::String& model_name, const godot::Dictionary& variables) {
	if (RmlContext* ctx = _resolve()) return ctx->create_data_model_from_dict(_model(model_name), variables);
	return false;
}

void RmlContextScope::update_data_model(const godot::String& model_name, const godot::Dictionary& variables) {
	if (RmlContext* ctx = _resolve()) ctx->update_data_model(_model(model_name), variables);
}

bool RmlContextScope::bind_data_array(const godot::String& model_name,
		const godot::String& array_name, const godot::Array& initial_array) {
	if (RmlContext* ctx = _resolve()) return ctx->bind_data_array(_model(model_name), array_name, initial_array);
	return false;
}

void RmlContextScope::set_data_array(const godot::String& model_name,
		const godot::String& array_name, const godot::Array& array) {
	if (RmlContext* ctx = _resolve()) ctx->set_data_array(_model(model_name), array_name, array);
}

void RmlContextScope::push_data_array_item(const godot::String& model_name,
		const godot::String& array_name, const godot::Variant& value) {
	if (RmlContext* ctx = _resolve()) ctx->push_data_array_item(_model(model_name), array_name, value);
}

void RmlContextScope::remove_data_array_item(const godot::String& model_name,
		const godot::String& array_name, int index) {
	if (RmlContext* ctx = _resolve()) ctx->remove_data_array_item(_model(model_name), array_name, index);
}

void RmlContextScope::set_data_array_item(const godot::String& model_name,
		const godot::String& array_name, int index, const godot::Variant& value) {
	if (RmlContext* ctx = _resolve()) ctx->set_data_array_item(_model(model_name), array_name, index, value);
}

int RmlContextScope::get_data_array_size(const godot::String& model_name, const godot::String& array_name) const {
	if (RmlContext* ctx = _resolve()) return ctx->get_data_array_size(_model(model_name), array_name);
	return 0;
}

void RmlContextScope::clear_data_array(const godot::String& model_name, const godot::String& array_name) {
	if (RmlContext* ctx = _resolve()) ctx->clear_data_array(_model(model_name), array_name);
}

godot::Ref<RmlDataModel> RmlContextScope::get_data_model_handle(const godot::String& model_name) {
	if (RmlContext* ctx = _resolve()) return ctx->get_data_model_handle(_model(model_name));
	godot::Ref<RmlDataModel> handle;
	handle.instantiate();
	return handle;
}

// --- Context-global pass-throughs ---

godot::Ref<RmlElementHandle> RmlContextScope::get_element_at_point(const godot::Vector2& point) const {
	if (RmlContext* ctx = _resolve()) return ctx->get_element_at_point(point);
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	return handle;
}

godot::Ref<RmlElementHandle> RmlContextScope::get_focused_element() const {
	if (RmlContext* ctx = _resolve()) return ctx->get_focused_element();
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	return handle;
}

godot::String RmlContextScope::get_hovered_element_id() const {
	if (RmlContext* ctx = _resolve()) return ctx->get_hovered_element_id();
	return {};
}

bool RmlContextScope::register_texture(const godot::String& name, const godot::Ref<godot::Texture2D>& texture) {
	if (RmlContext* ctx = _resolve()) return ctx->register_texture(name, texture);
	return false;
}

bool RmlContextScope::unregister_texture(const godot::String& name) {
	if (RmlContext* ctx = _resolve()) return ctx->unregister_texture(name);
	return false;
}

bool RmlContextScope::register_decorator_shader(const godot::String& name, const godot::Ref<godot::Shader>& shader) {
	if (RmlContext* ctx = _resolve()) return ctx->register_decorator_shader(name, shader);
	return false;
}

bool RmlContextScope::register_decorator_material(const godot::String& name, const godot::Ref<godot::ShaderMaterial>& material) {
	if (RmlContext* ctx = _resolve()) return ctx->register_decorator_material(name, material);
	return false;
}

bool RmlContextScope::unregister_decorator_shader(const godot::String& name) {
	if (RmlContext* ctx = _resolve()) return ctx->unregister_decorator_shader(name);
	return false;
}

bool RmlContextScope::inject_stylesheet(const godot::String& rcss_string) {
	if (RmlContext* ctx = _resolve()) return ctx->inject_stylesheet(rcss_string);
	return false;
}

} // namespace RmlGodot
