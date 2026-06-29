// RmlContext — embedded sub-documents (issue #56). See RmlContext.cpp for the TU map.
//
// One context can embed another authored document (.rml + its own <link> RCSS +
// its own <script>) as a real subtree of the parent's DOM. The embedded document
// is mounted as the child of an <embed-doc> host element, so it lays out as an
// ordinary box in the parent's layout tree — participating in the parent's
// flexbox / @media / anchoring / overflow — while keeping its own GDScript
// instance and inline-handler resolution.
//
// Why it works (verified against RmlUi core):
//   * Factory builds a document with owner_document == itself; Element::
//     SetOwnerDocument refuses to change a document's owner across reparenting,
//     so the embedded <script>/gdscript: handlers keep resolving correctly.
//   * The layout engine has no special-casing of ElementDocument, so once we
//     override the document's default position:absolute to an in-flow value it
//     formats as a normal flex/block child of the parent.
//   * Context::Update recurses the whole element tree, so the embed renders and
//     updates as part of the parent; only top-level documents are positioned as
//     free roots, which a nested document explicitly is not.
#include "RmlContext.hpp"
#include "RmlManager.hpp"
#include "RmlEmbedElement.hpp"
#include "RmlElementHandle.hpp"
#include "GodotScriptDocument.hpp"
#include "RmlContextInternal.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

namespace {

// Read a string attribute, or "" if absent.
std::string attr_str(Rml::Element* el, const char* name) {
	const Rml::Variant* v = el->GetAttribute(Rml::String(name));
	return v != nullptr ? std::string(v->Get<Rml::String>().c_str()) : std::string();
}

bool is_attr_boundary(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '<';
}

// Rewrite every `data-model="X"` attribute in an embed's source to a name unique
// to this embed (`<embed_id>::X`), so two instances of the same .rml bind to
// independent context data models. Collects the distinct namespaced names and
// the primary (first) one. Heuristic but safe for normal RML: it only rewrites
// at an attribute boundary followed by `= "..."` (so `data-model` in text/RCSS
// is left alone). Returns the rewritten source.
std::string namespace_data_models(const std::string& in, const std::string& embed_id,
		std::vector<std::string>& out_names, std::string& out_primary) {
	const std::string needle = "data-model";
	std::string out;
	out.reserve(in.size() + 64);
	size_t i = 0;
	while (true) {
		size_t pos = in.find(needle, i);
		if (pos == std::string::npos) {
			out.append(in, i, std::string::npos);
			break;
		}
		out.append(in, i, pos - i);
		out.append(needle);
		size_t j = pos + needle.size();

		const bool boundary_before = (pos == 0) || is_attr_boundary(in[pos - 1]);
		size_t k = j;
		while (k < in.size() && (in[k] == ' ' || in[k] == '\t' || in[k] == '\n' || in[k] == '\r')) k++;

		if (boundary_before && k < in.size() && in[k] == '=') {
			k++;
			while (k < in.size() && (in[k] == ' ' || in[k] == '\t' || in[k] == '\n' || in[k] == '\r')) k++;
			if (k < in.size() && (in[k] == '"' || in[k] == '\'')) {
				const char quote = in[k];
				const size_t vs = k + 1;
				const size_t ve = in.find(quote, vs);
				if (ve != std::string::npos) {
					const std::string local = in.substr(vs, ve - vs);
					const std::string ns = embed_id + "::" + local;
					if (std::find(out_names.begin(), out_names.end(), ns) == out_names.end())
						out_names.push_back(ns);
					if (out_primary.empty()) out_primary = ns;
					out.append(in, j, vs - j); // the `= "` (whitespace + '=' + opening quote)
					out.append(ns);
					out.push_back(quote);
					i = ve + 1;
					continue;
				}
			}
		}
		i = j;
	}
	return out;
}

// Collect the array names referenced by `data-for="item : array"` attributes.
// They are pre-bound (empty) before a namespaced embed loads, so the data-for
// view attaches to a bound array and every later update renders normally (a
// late-bound array does not reliably refresh an already-attached view).
void collect_data_for_arrays(const std::string& in, std::vector<std::string>& out) {
	const std::string needle = "data-for";
	auto is_ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
	size_t i = 0;
	while (true) {
		size_t pos = in.find(needle, i);
		if (pos == std::string::npos) break;
		size_t j = pos + needle.size();
		const bool boundary = (pos == 0) || is_attr_boundary(in[pos - 1]);
		size_t k = j;
		while (k < in.size() && is_ws(in[k])) k++;
		if (boundary && k < in.size() && in[k] == '=') {
			k++;
			while (k < in.size() && is_ws(in[k])) k++;
			if (k < in.size() && (in[k] == '"' || in[k] == '\'')) {
				const char q = in[k];
				const size_t vs = k + 1;
				const size_t ve = in.find(q, vs);
				if (ve != std::string::npos) {
					const std::string expr = in.substr(vs, ve - vs);
					const size_t colon = expr.rfind(':');
					if (colon != std::string::npos) {
						std::string arr = expr.substr(colon + 1);
						const size_t a = arr.find_first_not_of(" \t");
						const size_t b = arr.find_last_not_of(" \t");
						if (a != std::string::npos) {
							arr = arr.substr(a, b - a + 1);
							if (std::find(out.begin(), out.end(), arr) == out.end())
								out.push_back(arr);
						}
					}
					i = ve + 1;
					continue;
				}
			}
		}
		i = j;
	}
}

} // namespace

std::string RmlContext::_next_embed_id() {
	std::string candidate;
	do {
		candidate = std::string("embed_") + std::to_string(_embed_counter++);
	} while (_embeds.find(candidate) != _embeds.end());
	return candidate;
}

std::string RmlContext::_resolve_embed_src(Rml::Element* host, const std::string& src) const {
	godot::String s(src.c_str());
	if (s.begins_with("res://") || s.begins_with("user://") || s.begins_with("/")) {
		return src;
	}
	// Relative: resolve against the host's owning document directory (mirrors how
	// RmlUi resolves <link>/<script> src). RmlUi URL-encodes ':' as '|' in stored
	// source paths, so decode before using it as a Godot path.
	Rml::ElementDocument* owner = (host != nullptr) ? host->GetOwnerDocument() : nullptr;
	if (owner != nullptr) {
		godot::String base = godot::String(owner->GetSourceURL().c_str()).replace("|", ":");
		if (!base.is_empty()) {
			godot::String dir = base.get_base_dir();
			if (!dir.is_empty()) {
				return std::string(dir.path_join(s).utf8().get_data());
			}
		}
	}
	return src;
}

bool RmlContext::_mount_embed_core(RmlEmbedElement* host, const std::string& src_raw,
		const std::string& model, const std::string& embed_id, int depth,
		std::vector<std::string>& src_chain) {
	if (host == nullptr || _rml_context == nullptr) return false;

	if (depth > k_max_embed_depth) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Embed nesting too deep (> ") +
			godot::String::num_int64(k_max_embed_depth) +
			godot::String("), skipping: ") + godot::String(src_raw.c_str()));
		return false;
	}

	const std::string resolved = _resolve_embed_src(host, src_raw);

	// Cycle guard: a src already present in the current ancestor chain would
	// recurse forever.
	for (const auto& s : src_chain) {
		if (s == resolved) {
			godot::UtilityFunctions::push_warning(
				godot::String("[RmlUi] Embed cycle detected, skipping: ") +
				godot::String(resolved.c_str()));
			return false;
		}
	}

	// Build the embedded document through the document load path so its <script>
	// blocks compile and owner_document is set to itself. LoadDocument(FromMemory)
	// appends it to the context root; we immediately reparent it below.
	//
	// Namespacing (opt-in via <embed-doc model="...">): rewrite the embed's
	// data-model attributes to names unique to this embed and pre-create those
	// models, so two instances of the same .rml get independent data. Without it,
	// the embed binds to context models by their authored names (today's behavior).
	const bool namespaced = !model.empty();
	std::string primary_model;
	Rml::ElementDocument* doc = nullptr;

	if (namespaced) {
		godot::Ref<godot::FileAccess> f = godot::FileAccess::open(
			godot::String(resolved.c_str()), godot::FileAccess::READ);
		if (f.is_null()) {
			if (!console_log_muted()) {
				godot::UtilityFunctions::push_error(
					godot::String("[RmlUi] mount_embed — cannot open: ") + godot::String(resolved.c_str()));
			}
			return false;
		}
		const std::string src_text(f->get_as_text().utf8().get_data());
		f->close();

		std::vector<std::string> ns_names;
		const std::string rewritten = namespace_data_models(src_text, embed_id, ns_names, primary_model);
		// Pre-create each namespaced model (allow_missing, so scalar bindings can be
		// added after load and fed via the handle).
		for (const auto& nm : ns_names) {
			_create_data_model_impl(godot::String(nm.c_str()), true);
		}
		// Pre-bind data-for arrays (empty) on the primary model BEFORE the document
		// loads, so each data-for view attaches to a bound array and every later
		// update (set_array/push from the handle) renders reliably.
		if (!primary_model.empty()) {
			std::vector<std::string> for_arrays;
			collect_data_for_arrays(src_text, for_arrays);
			for (const auto& arr : for_arrays) {
				bind_data_array(godot::String(primary_model.c_str()),
					godot::String(arr.c_str()), godot::Array());
			}
		}
		doc = _rml_context->LoadDocumentFromMemory(
			Rml::String(rewritten.c_str()), Rml::String(resolved.c_str()));
	} else {
		doc = _rml_context->LoadDocument(Rml::String(resolved.c_str()));
	}

	if (doc == nullptr) {
		if (!console_log_muted()) {
			godot::UtilityFunctions::push_error(
				godot::String("[RmlUi] mount_embed — failed to load: ") +
				godot::String(resolved.c_str()));
		}
		return false;
	}

	// Non-namespaced embed: the primary model is the root's authored data-model
	// (a context-global model the parent created), used for the `data` handle.
	if (!namespaced) {
		primary_model = attr_str(doc, "data-model");
	}

	// Theme cascades in; the embed's own <link> styles stay local to its subtree.
	_apply_base_stylesheet(doc);

	// Reparent from the context root into the <embed-doc> host. RmlUi preserves a
	// document's owner_document across this move (Element::SetOwnerDocument is a
	// no-op when owner_document == this), so the embed's <script>/gdscript:
	// handlers keep resolving to the embedded document.
	Rml::Element* root = _rml_context->GetRootElement();
	Rml::ElementPtr doc_ptr = root->RemoveChild(doc);
	if (!doc_ptr) {
		godot::UtilityFunctions::push_error(
			"[RmlUi] mount_embed — could not detach embedded document from root");
		_rml_context->UnloadDocument(doc);
		return false;
	}
	host->AppendChild(std::move(doc_ptr));

	// A free document defaults to position:absolute (out of flow). Override to an
	// in-flow value so it lays out as an ordinary flex/block item: the host
	// <embed-doc> is the layout box; the document fills it.
	doc->SetProperty("position", "relative");

	// Make the embedded document visible without stealing focus from the parent.
	doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None, Rml::ScrollFlag::None);

	// Tag + register the host.
	host->SetId(Rml::String(embed_id.c_str()));
	host->set_embed_id(embed_id);
	host->set_mounted(true);

	EmbedEntry entry;
	entry.embed_id = embed_id;
	entry.src = resolved;
	entry.model = model;
	entry.data_model = primary_model;
	entry.host = host;
	entry.document = doc;
	_embeds[embed_id] = entry;

	// (The embed's <script> blocks self-resolve `var data` from the document's
	// data-model attribute at instantiation — see GodotScriptDocument; no
	// injection needed here. entry.data_model drives get_embedded_data.)

	// Recurse into any <embed-doc> authored inside this embed.
	src_chain.push_back(resolved);
	_mount_declarative_embeds(doc, depth + 1, src_chain);
	src_chain.pop_back();

	if (!console_log_muted()) {
		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Embedded '") + godot::String(embed_id.c_str()) +
			godot::String("' ← ") + godot::String(resolved.c_str()));
	}
	return true;
}

std::string RmlContext::_mount_embed_into(Rml::Element* parent, const std::string& src,
		const std::string& model, std::string embed_id) {
	if (parent == nullptr || _rml_context == nullptr) return {};

	if (embed_id.empty()) embed_id = _next_embed_id();
	if (_embeds.find(embed_id) != _embeds.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] mount_embed — embed id already in use: ") +
			godot::String(embed_id.c_str()));
		return {};
	}

	Rml::ElementDocument* owner = parent->GetOwnerDocument();
	if (owner == nullptr) {
		godot::UtilityFunctions::push_error(
			"[RmlUi] mount_embed — parent element has no owning document");
		return {};
	}

	// Create the <embed-doc> host and attach it to the parent BEFORE mounting, so
	// the embedded document's first layout sees a valid parent chain.
	Rml::ElementPtr host_ptr = owner->CreateElement("embed-doc");
	if (!host_ptr) {
		godot::UtilityFunctions::push_error("[RmlUi] mount_embed — failed to create <embed-doc> host");
		return {};
	}
	host_ptr->SetAttribute("src", Rml::String(src.c_str()));
	if (!model.empty()) host_ptr->SetAttribute("model", Rml::String(model.c_str()));

	Rml::Element* host_raw = parent->AppendChild(std::move(host_ptr));
	auto* host = rmlui_dynamic_cast<RmlEmbedElement*>(host_raw);
	if (host == nullptr) {
		// <embed-doc> instancer not registered — should never happen.
		parent->RemoveChild(host_raw);
		godot::UtilityFunctions::push_error("[RmlUi] mount_embed — <embed-doc> instancer missing");
		return {};
	}

	std::vector<std::string> src_chain;
	if (!_mount_embed_core(host, src, model, embed_id, 0, src_chain)) {
		parent->RemoveChild(host_raw);
		return {};
	}

	_sync_dimensions();
	_rml_context->Update();
	_render_dirty = true;
	return embed_id;
}

void RmlContext::_mount_declarative_embeds(Rml::Element* subtree_root, int depth,
		std::vector<std::string>& src_chain) {
	if (subtree_root == nullptr) return;

	// Capture matches first: GetElementsByTagName finds only the <embed-doc>
	// elements that already exist (a not-yet-mounted embed's sub-document, with
	// its own deeper <embed-doc>, hasn't been loaded yet), so nested levels are
	// reached through the recursion inside _mount_embed_core rather than here.
	Rml::ElementList hosts;
	subtree_root->GetElementsByTagName(hosts, "embed-doc");
	for (Rml::Element* host_el : hosts) {
		auto* host = rmlui_dynamic_cast<RmlEmbedElement*>(host_el);
		if (host == nullptr || host->is_mounted()) continue;

		const std::string src = attr_str(host, "src");
		if (src.empty()) continue;

		const std::string model = attr_str(host, "model");
		std::string embed_id(host->GetId().c_str());
		if (embed_id.empty() || _embeds.find(embed_id) != _embeds.end()) {
			embed_id = _next_embed_id();
		}
		_mount_embed_core(host, src, model, embed_id, depth, src_chain);
	}
}

godot::String RmlContext::mount_embed(const godot::String& parent_element_id,
		const godot::String& src, const godot::Dictionary& options) {
	_warn_if_off_main_thread();
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_error("[RmlUi] mount_embed — context not initialized");
		return {};
	}
	if (src.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] mount_embed — src is empty");
		return {};
	}

	Rml::Element* parent = _find_element(parent_element_id);
	if (parent == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] mount_embed — parent element not found: ") + parent_element_id);
		return {};
	}

	std::string embed_id;
	if (options.has("id")) {
		const godot::String id_opt = options["id"];
		embed_id = std::string(id_opt.utf8().get_data());
	}
	std::string model;
	if (options.has("model")) {
		const godot::String model_opt = options["model"];
		model = std::string(model_opt.utf8().get_data());
	}

	std::string id = _mount_embed_into(parent, std::string(src.utf8().get_data()), model, embed_id);
	return godot::String(id.c_str());
}

bool RmlContext::unmount_embed(const godot::String& embed_id) {
	_warn_if_off_main_thread();
	auto it = _embeds.find(std::string(embed_id.utf8().get_data()));
	if (it == _embeds.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] unmount_embed — unknown embed id: ") + embed_id);
		return false;
	}

	Rml::Element* host = it->second.host;
	if (host == nullptr) {
		_embeds.erase(it);
		return false;
	}

	// Removing the host destroys its whole subtree (the embedded document, plus
	// any nested embeds). Drop their registry entries and any listener records
	// pointing into the subtree first, to avoid dangling pointers.
	_purge_embeds_in_subtree(host);
	_purge_listener_records_in_subtree(host);

	if (Rml::Element* parent = host->GetParentNode()) {
		parent->RemoveChild(host); // discards the returned ElementPtr → destroys subtree
	}

	_render_dirty = true;
	if (_rml_context != nullptr) _rml_context->Update();
	return true;
}

bool RmlContext::reload_embed(const godot::String& embed_id) {
	_warn_if_off_main_thread();
	auto it = _embeds.find(std::string(embed_id.utf8().get_data()));
	if (it == _embeds.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] reload_embed — unknown embed id: ") + embed_id);
		return false;
	}

	// Capture what we need to re-mount under the same parent with the same id.
	Rml::Element* host = it->second.host;
	Rml::Element* parent = (host != nullptr) ? host->GetParentNode() : nullptr;
	const std::string id_copy = it->second.embed_id;
	const std::string src_copy = it->second.src;     // already resolved/absolute
	const std::string model_copy = it->second.model;
	if (parent == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] reload_embed — embed has no parent: ") + embed_id);
		return false;
	}

	if (!unmount_embed(embed_id)) return false;
	return !_mount_embed_into(parent, src_copy, model_copy, id_copy).empty();
}

godot::Variant RmlContext::get_embedded_script(const godot::String& embed_id) {
	godot::Array instances = get_embedded_scripts(embed_id);
	return instances.is_empty() ? godot::Variant() : instances[0];
}

godot::Array RmlContext::get_embedded_scripts(const godot::String& embed_id) {
	godot::Array result;
	auto it = _embeds.find(std::string(embed_id.utf8().get_data()));
	if (it == _embeds.end() || it->second.document == nullptr) return result;
	auto* doc = rmlui_dynamic_cast<GodotScriptDocument*>(it->second.document);
	if (doc != nullptr) {
		result.append_array(doc->get_script_instances());
	}
	return result;
}

godot::Ref<RmlElementHandle> RmlContext::get_embedded_element(const godot::String& embed_id,
		const godot::String& inner_id) const {
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();

	auto it = _embeds.find(std::string(embed_id.utf8().get_data()));
	if (it == _embeds.end() || it->second.document == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_embedded_element — unknown embed id: ") + embed_id);
		return handle;
	}

	// GetElementById on the embedded document searches from its own root (the
	// embed's owner-document is itself), so the lookup is scoped to this embed's
	// subtree — sibling embeds and the parent are not searched.
	Rml::Element* el = it->second.document->GetElementById(Rml::String(inner_id.utf8().get_data()));
	if (el != nullptr) {
		handle->set_element(el);
	} else {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_embedded_element — '") + inner_id +
			godot::String("' not found in embed '") + embed_id + godot::String("'"));
	}
	return handle;
}

godot::Ref<RmlDataModel> RmlContext::get_embedded_data(const godot::String& embed_id,
		const godot::String& model_name) {
	godot::Ref<RmlDataModel> handle;
	handle.instantiate();

	auto it = _embeds.find(std::string(embed_id.utf8().get_data()));
	if (it == _embeds.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_embedded_data — unknown embed id: ") + embed_id);
		return handle;
	}

	std::string resolved_model;
	if (model_name.is_empty()) {
		resolved_model = it->second.data_model;          // the embed's primary model
	} else if (!it->second.model.empty()) {
		resolved_model = it->second.embed_id + "::" +     // namespaced embed
			std::string(model_name.utf8().get_data());
	} else {
		resolved_model = std::string(model_name.utf8().get_data()); // authored/global name
	}

	if (resolved_model.empty()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_embedded_data — embed '") + embed_id +
			godot::String("' has no data model"));
		return handle;
	}
	handle->setup(this, resolved_model);
	return handle;
}

godot::Ref<RmlDataModel> RmlContext::get_data_model_handle(const godot::String& model_name) {
	godot::Ref<RmlDataModel> handle;
	handle.instantiate();
	handle->setup(this, std::string(model_name.utf8().get_data()));
	return handle;
}

godot::Ref<RmlElementHandle> RmlContext::get_element_at_point(const godot::Vector2& point) const {
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	if (_rml_context != nullptr) {
		Rml::Element* el = _rml_context->GetElementAtPoint(Rml::Vector2f(point.x, point.y));
		if (el != nullptr) handle->set_element(el);
	}
	return handle;
}

godot::Ref<RmlElementHandle> RmlContext::get_focused_element() const {
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	if (_rml_context != nullptr) {
		Rml::Element* el = _rml_context->GetFocusElement();
		if (el != nullptr) handle->set_element(el);
	}
	return handle;
}

void RmlContext::_redirty_embeds_media() {
	for (auto& [id, e] : _embeds) {
		if (e.document == nullptr) continue;
		const Rml::StyleSheetContainer* c = e.document->GetStyleSheetContainer();
		if (c == nullptr) continue;
		// Context::SetDimensions re-evaluates only top-level documents' @media;
		// re-setting a fresh copy of the embed's container (a new pointer) triggers
		// ElementDocument::DirtyMediaQueries against the new context dimensions.
		e.document->SetStyleSheetContainer(c->CombineStyleSheetContainer(Rml::StyleSheetContainer()));
	}
}

godot::PackedStringArray RmlContext::get_embedded_ids() const {
	godot::PackedStringArray ids;
	for (const auto& [id, entry] : _embeds) {
		ids.push_back(godot::String(id.c_str()));
	}
	return ids;
}

bool RmlContext::is_embed_mounted(const godot::String& embed_id) const {
	return _embeds.find(std::string(embed_id.utf8().get_data())) != _embeds.end();
}

void RmlContext::_purge_embeds_in_subtree(Rml::Element* root_host) {
	for (auto it = _embeds.begin(); it != _embeds.end(); ) {
		bool inside = false;
		for (Rml::Element* e = it->second.host; e != nullptr; e = e->GetParentNode()) {
			if (e == root_host) { inside = true; break; }
		}
		if (inside) {
			it = _embeds.erase(it);
		} else {
			++it;
		}
	}
}

void RmlContext::_purge_listener_records_in_subtree(Rml::Element* root_host) {
	auto it = _listener_records.begin();
	while (it != _listener_records.end()) {
		bool inside = false;
		for (Rml::Element* e = it->element; e != nullptr; e = e->GetParentNode()) {
			if (e == root_host) { inside = true; break; }
		}
		if (inside) {
			// The element is about to be destroyed; RmlUi fires listener OnDetach
			// on destruction, so just drop the record (no RemoveEventListener).
			it = _listener_records.erase(it);
		} else {
			++it;
		}
	}
}

void RmlContext::_update_embed_layout() {
	if (_embeds.empty() || _rml_context == nullptr) return;

	bool parent_dirtied = false;
	for (auto& [id, e] : _embeds) {
		if (e.document == nullptr || e.host == nullptr) continue;

		// Nested documents are skipped by Context::Update's root-only layout loop,
		// so format the embed's own subtree here. UpdateDocument() is cheap when
		// nothing changed — its internal UpdateLayout self-gates on the document's
		// layout-dirty flag (which we can't read directly: it is protected) and
		// UpdatePosition no-ops for a non-root document.
		e.document->UpdateDocument();

		// If the embed's outer size changed, the parent must reflow to reposition
		// the sibling widgets around it.
		const Rml::Vector2f size = e.document->GetBox().GetSize();
		if (size.x != e.last_w || size.y != e.last_h) {
			e.last_w = size.x;
			e.last_h = size.y;
			e.host->dirty_parent_layout();
			parent_dirtied = true;
			_render_dirty = true;
		}
	}

	if (parent_dirtied) {
		_rml_context->Update();
	}
}

} // namespace RmlGodot
