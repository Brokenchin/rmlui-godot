#include "GodotScriptDocument.hpp"
#include "GodotEventListener.hpp"
#include "RmlManager.hpp"
#include "RmlContext.hpp"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Event.h>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

namespace {

// Inline <script>/gdscript: blocks must stay inert in the editor unless the
// owning context opted in (the preview panel's "Run inline scripts" checkbox
// flips set_editor_scripts_enabled). This single predicate governs BOTH:
//   - compilation (issue #36): get_or_compile_script() calls GDScript::reload(),
//     which on a mid-edit syntax error (e.g. an unclosed array literal) triggers
//     a GDScript debugger break that FREEZES the editor. The diagnostics
//     validator reloads the document on every keystroke, so editing an inline
//     script would otherwise freeze the editor mid-edit.
//   - execution (issue #29): the blocks are RefCounted classes WE instantiate,
//     bypassing Godot's @tool rule, so a stray loop/blocking call would freeze.
// Inert at runtime (is_editor_hint() == false). A null node in the editor is
// treated as "not opted in" — conservative, and the registered context is
// resolvable by the time any document parses or dispatches.
bool inline_scripts_gated_in_editor(RmlContext* node) {
	auto* engine = godot::Engine::get_singleton();
	return engine != nullptr && engine->is_editor_hint() &&
		(node == nullptr || !node->is_editor_scripts_enabled());
}

} // namespace

// --- GodotScriptDocument ---

GodotScriptDocument::GodotScriptDocument(const Rml::String& tag) :
		Rml::ElementDocument(tag) {}

GodotScriptDocument::~GodotScriptDocument() = default;

void GodotScriptDocument::LoadInlineScript(const Rml::String& content,
	const Rml::String& source_path, int source_line) {

	auto* manager = RmlManager::get_singleton();
	if (manager == nullptr) return;

	// Editor gate (issue #36): skip compilation entirely when inline scripts are
	// gated in the editor. Compiling here would call GDScript::reload() on the
	// possibly-mid-edit source and break into the debugger, freezing the editor.
	// The preview rebuilds the document when the user toggles scripts on, so the
	// block recompiles then. XML/RCSS diagnostics are unaffected — only the
	// GDScript compile is withheld.
	RmlContext* gate_node = (GetContext() != nullptr)
		? manager->find_context_node(GetContext()) : nullptr;
	if (inline_scripts_gated_in_editor(gate_node)) {
		return;
	}

	// dedent(): the XML parser delivers the block with its .rml indentation,
	// which whitespace-sensitive GDScript rejects ("Unexpected Indent").
	// No `extends` needed — GDScript implicitly extends RefCounted.
	//
	// Newline padding: source_line is the <script> tag's line (1-based) and
	// the content starts on that same line, so prepending source_line-1 blank
	// lines makes every GDScript-reported line number — parse errors AND
	// runtime stack frames (gdscript://…gd:N) — equal the .rml file line.
	//
	// Compiled through the manager's source-keyed cache: unchanged blocks are
	// reused across documents and hot reloads (Godot retains every runtime
	// GDScript until shutdown, so per-load recompiles would accumulate).
	godot::String source = godot::String(content.c_str()).dedent();
	if (source_line > 1) {
		source = godot::String("\n").repeat(source_line - 1) + source;
	}
	godot::Ref<godot::GDScript> script = manager->get_or_compile_script(source);
	if (script.is_null()) {
		// Routed through notify_log so the editor diagnostics' error bar can
		// show it on the <script> line (the trailing ': N.' carries the line);
		// the engine's own SCRIPT ERROR with the GDScript detail follows in
		// the Output log regardless.
		godot::String msg = godot::String(
			"Failed to compile <script> block (see Output for the GDScript error) in ") +
			godot::String(source_path.c_str()) + godot::String(": ") +
			godot::String::num_int64(source_line) + godot::String(".");
		if (!manager->is_console_log_muted()) {
			godot::UtilityFunctions::push_error(godot::String("[RmlUi] ") + msg);
		}
		manager->notify_log(1 /*Rml::Log::LT_ERROR*/, msg);
		return;
	}

	ScriptBlock block;
	block.script = script;
	_blocks.push_back(std::move(block));
}

void GodotScriptDocument::LoadExternalScript(const Rml::String& source_path) {
	// RmlUi stores every external resource path with ':' URL-encoded as '|'
	// (see XMLNodeHandlerHead::Absolutepath / DocumentHeader::MergePaths), so
	// the document's "res://" scheme arrives here as "res|//…". The RCSS
	// <link href> loader works because its path is routed through
	// StreamFile::Open, which decodes '|' back to ':' before touching the file
	// interface; <script src> paths skip that and reach LoadExternalScript()
	// still encoded. Decode them the same way (matching StreamFile and
	// GodotFileInterface) so the Godot ResourceLoader gets a valid path.
	//
	// This also gives <script src> the same resolution semantics as
	// <link href>: RmlUi has already joined a *relative* src against the
	// document's directory, and an absolute "res://…"/"user://…" src passes
	// through JoinPath untouched — so both forms land here correctly.
	godot::String gd_path = godot::String(source_path.c_str()).replace("|", ":");

	// Fall back to res:// when no scheme survives (e.g. a src that couldn't be
	// joined against the document path), keeping ResourceLoader happy — same
	// default GodotFileInterface::Open applies to file reads.
	if (!gd_path.begins_with("res://") && !gd_path.begins_with("user://") &&
		!gd_path.begins_with("/")) {
		gd_path = godot::String("res://") + gd_path;
	}

	godot::Ref<godot::Resource> res =
		godot::ResourceLoader::get_singleton()->load(gd_path);
	godot::Ref<godot::GDScript> script = res;
	if (script.is_null()) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] <script src> is not a GDScript: ") + gd_path);
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

godot::Array GodotScriptDocument::get_script_instances() {
	godot::Array result;
	for (auto& block : _blocks) {
		godot::Object* obj = _ensure_instance(block);
		if (obj != nullptr) {
			result.append(block.instance);
		}
	}
	return result;
}

godot::Object* GodotScriptDocument::_ensure_instance(ScriptBlock& block) {
	if (block.instance_failed || block.script.is_null()) return nullptr;

	godot::Object* obj = block.instance.operator godot::Object*();
	if (obj != nullptr) return obj;

	// Resolve the owning node once — needed both for the editor gate below and
	// for the rml_context injection further down.
	auto* manager = RmlManager::get_singleton();
	RmlContext* node = (manager != nullptr && GetContext() != nullptr)
		? manager->find_context_node(GetContext()) : nullptr;

	// Editor gate (issues #29/#36): never instantiate/run inline-script blocks in
	// the editor unless this context opted in. With the gate active the block was
	// also never compiled (LoadInlineScript skips it), so there is nothing to
	// instantiate here anyway — this stays as the explicit execution guard.
	if (inline_scripts_gated_in_editor(node)) {
		return nullptr;
	}

	block.instance = block.script->call("new");
	obj = block.instance.operator godot::Object*();
	if (obj == nullptr) {
		block.instance_failed = true;
		godot::UtilityFunctions::push_error("[RmlUi] Failed to instantiate <script> block");
		return nullptr;
	}

	// Inject the owning RmlContext into `var rml_context` if declared
	// (Object::set is a silent no-op for undeclared properties).
	if (node != nullptr) {
		obj->set("rml_context", node);

		// Issue #56: inject `var data` — a cached handle to this document's root
		// data-model (the namespaced name for an embed, the authored name for a
		// top-level document). Self-resolved here from the document's own
		// data-model attribute, so it is available during on_load and stays
		// consistent whether this is a root document or an embed.
		const Rml::Variant* dm = GetAttribute(Rml::String("data-model"));
		if (dm != nullptr) {
			const std::string model_name(dm->Get<Rml::String>().c_str());
			if (!model_name.empty()) {
				godot::Ref<RmlDataModel> data_handle;
				data_handle.instantiate();
				data_handle->setup(node, model_name);
				obj->set("data", data_handle);
			}
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

	auto* manager = RmlManager::get_singleton();
	Rml::Context* rml_ctx = element->GetContext();
	RmlContext* node = (manager != nullptr && rml_ctx != nullptr)
		? manager->find_context_node(rml_ctx) : nullptr;

	// Inline-script execution gate (issues #29/#36): the document's <script>
	// blocks are RefCounted classes WE instantiate, bypassing Godot's @tool rule,
	// so in the editor they must stay inert unless the context opted in. The node
	// / parent fallbacks below are ordinary attached GDScript whose own @tool-ness
	// already governs editor execution — left untouched.
	const bool inline_scripts_gated = inline_scripts_gated_in_editor(node);

	// 1. The document's own <script> blocks (skipped when gated in the editor;
	//    _ensure_instance would no-op anyway, but skipping is explicit).
	if (!inline_scripts_gated) {
		auto* doc = rmlui_dynamic_cast<GodotScriptDocument*>(element->GetOwnerDocument());
		if (doc != nullptr && doc->dispatch_to_scripts(method, args)) {
			return;
		}
	}

	// 2./3. The RmlContext node's attached script, then its parent node.
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

	// Suppress the not-found warning when the intended target was a gated inline
	// handler — otherwise every onload/onclick in a preview spams the log.
	if (inline_scripts_gated) return;

	godot::UtilityFunctions::push_warning(
		godot::String("[RmlUi] gdscript handler not found: ") + method +
		godot::String(" (looked in <script> blocks, the RmlContext node and its parent)"));
}

void GodotInlineScriptListener::OnDetach(Rml::Element* /*element*/) {
	// Instancer-created listener — owns itself, freed when detached.
	delete this;
}

} // namespace RmlGodot
