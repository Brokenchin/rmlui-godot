// RmlContext — element access, DOM events, textures, decorators (see RmlContext.cpp for the TU map).
#include "RmlContext.hpp"
#include "RmlManager.hpp"
#include "RmlElementHandle.hpp"
#include "GodotEventListener.hpp"
#include "GodotFontInterface.hpp"
#include "GodotScriptDocument.hpp"

#include <algorithm>
#include <utility>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StyleSheetContainer.h>
#include <RmlUi/Debugger.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector4.hpp>

namespace RmlGodot {

bool RmlContext::register_custom_element(const godot::String& tag_name,
	const godot::Callable& on_create, const godot::Callable& on_attribute_change) {

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot register custom element — RmlUI not initialized");
		return false;
	}

	if (!on_create.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot register custom element — on_create callable is invalid");
		return false;
	}

	std::string tag(tag_name.utf8().get_data());

	bool already_registered = false;
	auto& tags = manager->get_registered_tags();
	for (const auto& t : tags) {
		if (t == tag) {
			already_registered = true;
			break;
		}
	}

	RmlGodot::GodotElementInstancer::TagCallbacks callbacks;
	callbacks.on_create = on_create;
	callbacks.on_attribute_change = on_attribute_change;
	manager->get_element_instancer().register_tag(tag, std::move(callbacks));

	if (!already_registered) {
		Rml::Factory::RegisterElementInstancer(Rml::String(tag), &manager->get_element_instancer());
		tags.push_back(tag);
	}

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Custom element registered: <") + tag_name + godot::String(">"));
	return true;
}

// --- Phase 1: DOM events & element access ---

Rml::Element* RmlContext::_find_element(const godot::String& id) const {
	if (_rml_context == nullptr) return nullptr;

	Rml::String rml_id(id.utf8().get_data());
	int num_docs = _rml_context->GetNumDocuments();
	for (int i = 0; i < num_docs; i++) {
		Rml::ElementDocument* doc = _rml_context->GetDocument(i);
		if (doc == nullptr) continue;
		Rml::Element* el = doc->GetElementById(rml_id);
		if (el != nullptr) return el;
	}
	return nullptr;
}

// The bound DOM methods below are the embed_id == "" (context-global) case of
// the scoped variants — root documents and external game code reach them. An
// embedded document's <script> reaches the *_scoped forms through RmlContextScope
// (issue #59), which passes its own embed id so resolution stays inside the
// embed's subtree. Keeping one implementation per method (the scoped one) means
// the global path is byte-identical to before.

bool RmlContext::add_event_listener(const godot::String& element_id,
	const godot::String& event_type, const godot::Callable& callable, bool in_capture_phase) {
	return add_event_listener_scoped(std::string(), element_id, event_type, callable, in_capture_phase);
}

bool RmlContext::add_event_listener_scoped(const std::string& embed_id, const godot::String& element_id,
	const godot::String& event_type, const godot::Callable& callable, bool in_capture_phase) {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] add_event_listener — element not found: ") + element_id);
		return false;
	}

	std::string type_str(event_type.utf8().get_data());
	auto* listener = new RmlGodot::GodotEventListener(callable, type_str);
	el->AddEventListener(Rml::String(type_str), listener, in_capture_phase);

	ListenerRecord record;
	record.element = el;
	record.listener = listener;
	record.event_type = type_str;
	record.in_capture_phase = in_capture_phase;
	_listener_records.push_back(record);

	return true;
}

void RmlContext::remove_event_listeners(const godot::String& element_id,
	const godot::String& event_type) {
	remove_event_listeners_scoped(std::string(), element_id, event_type);
}

void RmlContext::remove_event_listeners_scoped(const std::string& embed_id,
	const godot::String& element_id, const godot::String& event_type) {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] remove_event_listeners — element not found: ") + element_id);
		return;
	}

	std::string type_str(event_type.utf8().get_data());

	auto it = _listener_records.begin();
	while (it != _listener_records.end()) {
		if (it->element == el && it->event_type == type_str) {
			el->RemoveEventListener(Rml::String(type_str), it->listener, it->in_capture_phase);
			it = _listener_records.erase(it);
		} else {
			++it;
		}
	}
}

godot::Ref<RmlElementHandle> RmlContext::get_element_by_id(const godot::String& id) const {
	return get_element_by_id_scoped(std::string(), id);
}

godot::Ref<RmlElementHandle> RmlContext::get_element_by_id_scoped(const std::string& embed_id,
	const godot::String& id) const {

	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();

	if (Rml::Element* el = _find_element_scoped(embed_id, id); el != nullptr) {
		handle->set_element(el);
	} else {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_element_by_id — element not found: ") + id);
	}

	return handle;
}

bool RmlContext::set_element_property(const godot::String& element_id,
	const godot::String& property, const godot::String& value) {
	return set_element_property_scoped(std::string(), element_id, property, value);
}

bool RmlContext::set_element_property_scoped(const std::string& embed_id, const godot::String& element_id,
	const godot::String& property, const godot::String& value) {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_property — element not found: ") + element_id);
		return false;
	}

	bool ok = el->SetProperty(
		Rml::String(property.utf8().get_data()),
		Rml::String(value.utf8().get_data()));
	if (ok) _render_dirty = true;
	return ok;
}

void RmlContext::remove_element_property(const godot::String& element_id,
	const godot::String& property) {
	remove_element_property_scoped(std::string(), element_id, property);
}

void RmlContext::remove_element_property_scoped(const std::string& embed_id,
	const godot::String& element_id, const godot::String& property) {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] remove_element_property — element not found: ") + element_id);
		return;
	}

	el->RemoveProperty(Rml::String(property.utf8().get_data()));
	_render_dirty = true;
}

void RmlContext::set_element_class(const godot::String& element_id,
	const godot::String& class_name, bool activate) {
	set_element_class_scoped(std::string(), element_id, class_name, activate);
}

void RmlContext::set_element_class_scoped(const std::string& embed_id,
	const godot::String& element_id, const godot::String& class_name, bool activate) {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_class — element not found: ") + element_id);
		return;
	}

	el->SetClass(Rml::String(class_name.utf8().get_data()), activate);
	_render_dirty = true;
}

void RmlContext::set_element_inner_rml(const godot::String& element_id, const godot::String& rml) {
	set_element_inner_rml_scoped(std::string(), element_id, rml);
}

void RmlContext::set_element_inner_rml_scoped(const std::string& embed_id,
	const godot::String& element_id, const godot::String& rml) {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_inner_rml — element not found: ") + element_id);
		return;
	}

	el->SetInnerRML(Rml::String(rml.utf8().get_data()));
	_render_dirty = true;
}

godot::String RmlContext::get_element_outer_rml(const godot::String& element_id) const {
	return get_element_outer_rml_scoped(std::string(), element_id);
}

godot::String RmlContext::get_element_outer_rml_scoped(const std::string& embed_id,
	const godot::String& element_id) const {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_element_outer_rml — element not found: ") + element_id);
		return {};
	}

	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	handle->set_element(el);
	return handle->get_outer_rml();
}

godot::String RmlContext::get_element_attribute(const godot::String& element_id,
	const godot::String& attribute, const godot::String& default_value) const {
	return get_element_attribute_scoped(std::string(), element_id, attribute, default_value);
}

godot::String RmlContext::get_element_attribute_scoped(const std::string& embed_id,
	const godot::String& element_id, const godot::String& attribute, const godot::String& default_value) const {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) return default_value;

	const Rml::Variant* attr = el->GetAttribute(Rml::String(attribute.utf8().get_data()));
	if (attr == nullptr) return default_value;
	return godot::String(attr->Get<Rml::String>().c_str());
}

void RmlContext::set_element_attribute(const godot::String& element_id,
	const godot::String& attribute, const godot::String& value) {
	set_element_attribute_scoped(std::string(), element_id, attribute, value);
}

void RmlContext::set_element_attribute_scoped(const std::string& embed_id,
	const godot::String& element_id, const godot::String& attribute, const godot::String& value) {

	Rml::Element* el = _find_element_scoped(embed_id, element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_attribute — element not found: ") + element_id);
		return;
	}

	el->SetAttribute(
		Rml::String(attribute.utf8().get_data()),
		Rml::String(value.utf8().get_data()));
	_render_dirty = true;
}

// --- Texture registration ---

bool RmlContext::register_texture(const godot::String& name, const godot::Ref<godot::Texture2D>& texture) {
	if (!texture.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] register_texture — texture is null");
		return false;
	}
	std::string key(name.utf8().get_data());
	if (!_render_interface.register_texture(key, texture)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] register_texture failed: ") + name);
		return false;
	}
	return true;
}

bool RmlContext::unregister_texture(const godot::String& name) {
	std::string key(name.utf8().get_data());
	return _render_interface.unregister_texture(key);
}

// --- Decorator shader registration ---

bool RmlContext::register_decorator_shader(const godot::String& name, const godot::Ref<godot::Shader>& shader) {
	if (!shader.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] register_decorator_shader — shader is null");
		return false;
	}
	std::string key(name.utf8().get_data());
	if (!_render_interface.register_shader(key, shader)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] register_decorator_shader failed: ") + name);
		return false;
	}
	return true;
}

bool RmlContext::register_decorator_material(const godot::String& name, const godot::Ref<godot::ShaderMaterial>& material) {
	if (!material.is_valid() || !material->get_shader().is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] register_decorator_material — material or its shader is null");
		return false;
	}
	std::string key(name.utf8().get_data());
	if (!_render_interface.register_shader_material(key, material)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] register_decorator_material failed: ") + name);
		return false;
	}
	return true;
}

bool RmlContext::unregister_decorator_shader(const godot::String& name) {
	std::string key(name.utf8().get_data());
	return _render_interface.unregister_shader(key);
}

// --- A4: Drag-and-drop (gd_drag interop) ---

} // namespace RmlGodot
