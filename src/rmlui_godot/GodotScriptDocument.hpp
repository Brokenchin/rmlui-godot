#pragma once

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/EventListener.h>

#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <string>
#include <vector>

namespace RmlGodot {

/// ElementDocument subclass adding inline GDScript support. Instanced for the
/// "body" tag via GodotScriptDocumentInstancer, so every document loaded by
/// any context gets it.
///
/// <script> blocks compile at document load (errors surface immediately, with
/// the document's path in the message). Instances are created lazily on the
/// first dispatched event — so merely loading a document never runs user code.
/// A script declaring `var rml_context` receives the owning RmlContext node
/// before its first handler call.
class GodotScriptDocument final : public Rml::ElementDocument {
public:
	RMLUI_RTTI_DefineWithParent(GodotScriptDocument, Rml::ElementDocument)

	explicit GodotScriptDocument(const Rml::String& tag);
	~GodotScriptDocument() override;

	void LoadInlineScript(const Rml::String& content, const Rml::String& source_path, int source_line) override;
	void LoadExternalScript(const Rml::String& source_path) override;

	/// Call `method(args...)` on the first script block that defines it.
	/// Returns false when no block has the method.
	bool dispatch_to_scripts(const godot::String& method, const godot::Array& args);

	bool has_scripts() const { return !_blocks.empty(); }

private:
	struct ScriptBlock {
		godot::Ref<godot::GDScript> script;
		godot::Variant instance; // created lazily; NIL until first dispatch
		bool instance_failed = false;
	};
	std::vector<ScriptBlock> _blocks;

	godot::Object* _ensure_instance(ScriptBlock& block);
};

/// Replaces RmlUi's default "body" instancer so documents become
/// GodotScriptDocument. Registered by RmlManager after Rml::Initialise().
class GodotScriptDocumentInstancer final : public Rml::ElementInstancer {
public:
	Rml::ElementPtr InstanceElement(Rml::Element* parent, const Rml::String& tag,
		const Rml::XMLAttributes& attributes) override;
	void ReleaseElement(Rml::Element* element) override;
};

/// Listener behind inline event attributes: onclick="gdscript:method_name".
/// Resolution order at event time:
///   1. the document's <script> block instances
///   2. the owning RmlContext node (its attached GDScript)
///   3. the RmlContext's parent node
/// The handler receives the event Dictionary as its single argument.
class GodotInlineScriptListener final : public Rml::EventListener {
public:
	explicit GodotInlineScriptListener(std::string method) : _method(std::move(method)) {}

	void ProcessEvent(Rml::Event& event) override;
	void OnDetach(Rml::Element* element) override;

private:
	std::string _method;
};

} // namespace RmlGodot
