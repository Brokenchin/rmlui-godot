#include "GodotScriptDocument.hpp"
#include "GodotEventListener.hpp"
#include "RmlManager.hpp"
#include "RmlContext.hpp"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Event.h>

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

// --- GodotScriptDocument ---

GodotScriptDocument::GodotScriptDocument(const Rml::String& tag) :
		Rml::ElementDocument(tag) {}

GodotScriptDocument::~GodotScriptDocument() = default;

void GodotScriptDocument::LoadInlineScript(const Rml::String& content,
	const Rml::String& source_path, int source_line) {

	godot::Ref<godot::GDScript> script;
	script.instantiate();
	// GDScript without an `extends` implicitly extends RefCounted — blocks
	// don't need boilerplate, and line numbers in errors match the source.
	script->set_source_code(godot::String(content.c_str()));

	godot::Error err = script->reload();
	if (err != godot::OK) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to compile <script> block in ") +
			godot::String(source_path.c_str()) + godot::String(" (line ") +
			godot::String::num_int64(source_line) + godot::String(")"));
		return;
	}

	ScriptBlock block;
	block.script = script;
	_blocks.push_back(std::move(block));
}

void GodotScriptDocument::LoadExternalScript(const Rml::String& source_path) {
	godot::Ref<godot::Resource> res =
		godot::ResourceLoader::get_singleton()->load(godot::String(source_path.c_str()));
	godot::Ref<godot::GDScript> script = res;
	if (script.is_null()) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] <script src> is not a GDScript: ") +
			godot::String(source_path.c_str()));
		return;
	}

	ScriptBlock block;
	block.script = script;
	_blocks.push_back(std::move(block));
}

bool GodotScriptDocument::dispatch_to_scripts(const godot::String& method, const godot::Array& args) {
	for (auto& block : _blocks) {
		godot::Object* obj = _ensure_instance(block);
		if (obj != nullptr && obj->has_method(method)) {
			obj->callv(method, args);
			return true;
		}
	}
	return false;
}

godot::Object* GodotScriptDocument::_ensure_instance(ScriptBlock& block) {
	if (block.instance_failed || block.script.is_null()) return nullptr;

	godot::Object* obj = block.instance.operator godot::Object*();
	if (obj != nullptr) return obj;

	block.instance = block.script->call("new");
	obj = block.instance.operator godot::Object*();
	if (obj == nullptr) {
		block.instance_failed = true;
		godot::UtilityFunctions::push_error("[RmlUi] Failed to instantiate <script> block");
		return nullptr;
	}

	// Inject the owning RmlContext into `var rml_context` if declared
	// (Object::set is a silent no-op for undeclared properties).
	auto* manager = RmlManager::get_singleton();
	if (manager != nullptr && GetContext() != nullptr) {
		RmlContext* node = manager->find_context_node(GetContext());
		if (node != nullptr) {
			obj->set("rml_context", node);
		}
	}
	return obj;
}

// --- GodotScriptDocumentInstancer ---

Rml::ElementPtr GodotScriptDocumentInstancer::InstanceElement(Rml::Element* /*parent*/,
	const Rml::String& tag, const Rml::XMLAttributes& /*attributes*/) {
	return Rml::ElementPtr(new GodotScriptDocument(tag));
}

void GodotScriptDocumentInstancer::ReleaseElement(Rml::Element* element) {
	delete element;
}

// --- GodotInlineScriptListener ---

void GodotInlineScriptListener::ProcessEvent(Rml::Event& event) {
	const godot::String method(_method.c_str());
	godot::Array args;
	args.append(GodotEventListener::build_event_dict(event));

	Rml::Element* element = event.GetCurrentElement();
	if (element == nullptr) return;

	// 1. The document's own <script> blocks.
	auto* doc = rmlui_dynamic_cast<GodotScriptDocument*>(element->GetOwnerDocument());
	if (doc != nullptr && doc->dispatch_to_scripts(method, args)) {
		return;
	}

	// 2./3. The RmlContext node's attached script, then its parent node.
	auto* manager = RmlManager::get_singleton();
	Rml::Context* rml_ctx = element->GetContext();
	RmlContext* node = (manager != nullptr && rml_ctx != nullptr)
		? manager->find_context_node(rml_ctx) : nullptr;
	if (node != nullptr) {
		if (node->has_method(method)) {
			node->callv(method, args);
			return;
		}
		godot::Node* parent = node->get_parent();
		if (parent != nullptr && parent->has_method(method)) {
			parent->callv(method, args);
			return;
		}
	}

	godot::UtilityFunctions::push_warning(
		godot::String("[RmlUi] gdscript handler not found: ") + method +
		godot::String(" (looked in <script> blocks, the RmlContext node and its parent)"));
}

void GodotInlineScriptListener::OnDetach(Rml::Element* /*element*/) {
	// Instancer-created listener — owns itself, freed when detached.
	delete this;
}

} // namespace RmlGodot
