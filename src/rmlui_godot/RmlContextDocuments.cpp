// RmlContext — document/font/stylesheet management (see RmlContext.cpp for the TU map).
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

#include "RmlContextInternal.hpp"

namespace RmlGodot {

Rml::SharedPtr<Rml::StyleSheetContainer> RmlContext::_get_effective_base_sheet() {
	if (_has_local_base_rcss) {
		if (_local_base_rcss.empty()) return nullptr;
		return Rml::Factory::InstanceStyleSheetString(Rml::String(_local_base_rcss.c_str()));
	}
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (!manager) return nullptr;
	return manager->get_default_sheet();
}

void RmlContext::_apply_base_stylesheet(Rml::ElementDocument* doc) {
	if (!_use_default_rcss || doc == nullptr) return;

	auto base = _get_effective_base_sheet();
	if (!base) return;

	const Rml::StyleSheetContainer* doc_sheet = doc->GetStyleSheetContainer();
	if (doc_sheet) {
		auto combined = base->CombineStyleSheetContainer(*doc_sheet);
		doc->SetStyleSheetContainer(std::move(combined));
	} else {
		doc->SetStyleSheetContainer(base);
	}
}

void RmlContext::set_base_rcss(const godot::String& rcss) {
	_local_base_rcss = std::string(rcss.utf8().get_data());
	_has_local_base_rcss = true;
}

godot::String RmlContext::get_base_rcss() const {
	if (_has_local_base_rcss)
		return godot::String(_local_base_rcss.c_str());
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager) return manager->get_default_rcss();
	return {};
}

void RmlContext::append_base_rcss(const godot::String& rcss) {
	if (!_has_local_base_rcss) {
		auto* manager = RmlGodot::RmlManager::get_singleton();
		if (manager)
			_local_base_rcss = std::string(manager->get_default_rcss().utf8().get_data());
	}
	_local_base_rcss += "\n";
	_local_base_rcss += std::string(rcss.utf8().get_data());
	_has_local_base_rcss = true;
}

void RmlContext::reset_base_rcss() {
	_local_base_rcss.clear();
	_has_local_base_rcss = false;
}

void RmlContext::load_document(const godot::String& path) {
	_warn_if_off_main_thread();
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load document — context not initialized");
		return;
	}

	if (path.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load document — path is empty");
		return;
	}

	Rml::String rml_path(path.utf8().get_data());

	Rml::ElementDocument* doc = _rml_context->LoadDocument(rml_path);
	if (doc != nullptr) {
		_apply_base_stylesheet(doc);
		doc->Show();
		_sync_dimensions();
		_rml_context->Update();
		_render_dirty = true;

		_loaded_documents.push_back({std::string(rml_path.c_str()), doc});

		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Document loaded: ") + path);
	} else {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to load document: ") + path);
	}
}

bool RmlContext::load_document_from_string(const godot::String& rml_text, const godot::String& alias_path) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load document — context not initialized");
		return false;
	}

	if (rml_text.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load document — text is empty");
		return false;
	}

	Rml::ElementDocument* doc = _rml_context->LoadDocumentFromMemory(
		Rml::String(rml_text.utf8().get_data()),
		Rml::String(alias_path.utf8().get_data()));
	if (doc == nullptr) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to load document from string (") + alias_path + ")");
		return false;
	}

	_apply_base_stylesheet(doc);
	doc->Show();
	_sync_dimensions();
	_rml_context->Update();
	_render_dirty = true;

	_loaded_documents.push_back({std::string(alias_path.utf8().get_data()), doc});
	return true;
}

bool RmlContext::reload_document(const godot::String& path) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot reload document — context not initialized");
		return false;
	}

	const Rml::String rml_path(path.utf8().get_data());
	std::string path_str(rml_path.c_str());

	auto it = std::find_if(_loaded_documents.begin(), _loaded_documents.end(),
		[&](const LoadedDocument& ld) { return ld.path == path_str; });

	if (it == _loaded_documents.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Document not loaded, cannot reload: ") + path);
		return false;
	}

	_listener_records.clear();

	if (it->document != nullptr) {
		_rml_context->UnloadDocument(it->document);
	}

	Rml::ElementDocument* new_doc = _rml_context->LoadDocument(rml_path);
	if (new_doc == nullptr) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to reload document: ") + path);
		_loaded_documents.erase(it);
		return false;
	}

	it->document = new_doc;
	_apply_base_stylesheet(new_doc);
	new_doc->Show();

	for (auto& [name, entry] : _data_models) {
		entry.handle.DirtyAllVariables();
	}

	_sync_dimensions();
	_rml_context->Update();
	_render_dirty = true;

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Document reloaded: ") + path);
	return true;
}

void RmlContext::reload_all_documents() {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot reload — context not initialized");
		return;
	}

	_listener_records.clear();

	for (auto& ld : _loaded_documents) {
		if (ld.document != nullptr) {
			_rml_context->UnloadDocument(ld.document);
			ld.document = nullptr;
		}
	}

	for (auto& ld : _loaded_documents) {
		Rml::ElementDocument* doc = _rml_context->LoadDocument(Rml::String(ld.path));
		if (doc != nullptr) {
			_apply_base_stylesheet(doc);
			doc->Show();
			ld.document = doc;
		} else {
			godot::UtilityFunctions::push_error(
				godot::String("[RmlUi] Failed to reload document: ") + godot::String(ld.path.c_str()));
		}
	}

	auto before_size = _loaded_documents.size();
	_loaded_documents.erase(
		std::remove_if(_loaded_documents.begin(), _loaded_documents.end(),
			[](const LoadedDocument& ld) { return ld.document == nullptr; }),
		_loaded_documents.end());

	if (_loaded_documents.size() < before_size) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Removed ") +
			godot::String::num_int64(static_cast<int64_t>(before_size - _loaded_documents.size())) +
			godot::String(" failed document(s) during reload"));
	}

	for (auto& [name, entry] : _data_models) {
		entry.handle.DirtyAllVariables();
	}

	_sync_dimensions();
	_rml_context->Update();
	_render_dirty = true;

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] All documents reloaded (") +
		godot::String::num_int64(static_cast<int64_t>(_loaded_documents.size())) +
		godot::String(" documents)"));
}

godot::Array RmlContext::get_loaded_documents() const {
	godot::Array result;
	for (const auto& ld : _loaded_documents) {
		result.append(godot::String(ld.path.c_str()));
	}
	return result;
}

godot::Variant RmlContext::get_document_script(const godot::String& document_path) {
	const std::string wanted(document_path.utf8().get_data());
	for (const auto& ld : _loaded_documents) {
		if (!wanted.empty() && ld.path != wanted) continue;
		auto* doc = rmlui_dynamic_cast<GodotScriptDocument*>(ld.document);
		if (doc == nullptr) continue;
		godot::Array instances = doc->get_script_instances();
		if (!instances.is_empty()) {
			return instances[0];
		}
	}
	return godot::Variant();
}

bool RmlContext::load_font_face(const godot::String& path) {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load font — RmlUI not initialized");
		return false;
	}

	if (path.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load font — path is empty");
		return false;
	}

	Rml::String rml_path(path.utf8().get_data());
	bool ok = Rml::LoadFontFace(rml_path);
	if (ok) {
		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Font loaded: ") + path);
	} else {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to load font: ") + path);
	}
	return ok;
}

bool RmlContext::load_font_face_ex(const godot::String& path, const godot::String& family,
	int style, int weight, bool fallback) {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load font — RmlUI not initialized");
		return false;
	}
	if (path.is_empty() || family.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] load_font_face_ex: path and family are required");
		return false;
	}

	Rml::String rml_path(path.utf8().get_data());
	Rml::String rml_family(family.utf8().get_data());
	auto rml_style = (style == 1) ? Rml::Style::FontStyle::Italic : Rml::Style::FontStyle::Normal;
	auto rml_weight = (weight > 0 && weight <= 1000)
		? static_cast<Rml::Style::FontWeight>(weight)
		: Rml::Style::FontWeight::Auto;

	bool ok = Rml::LoadFontFace(rml_path, rml_family, rml_style, rml_weight, fallback);
	if (ok) {
		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Font loaded: ") + family +
			godot::String(" (from ") + path + godot::String(")"));
	} else {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to load font: ") + path);
	}
	return ok;
}

bool RmlContext::load_font_resource(const godot::Ref<godot::Font>& font) {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load font — RmlUI not initialized");
		return false;
	}
	if (!font.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load font — null resource");
		return false;
	}

	auto& fi = manager->get_font_interface();
	godot::TypedArray<godot::RID> rids = font->get_rids();

	if (rids.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Font resource has no TextServer RIDs");
		return false;
	}

	bool any_ok = false;
	for (int i = 0; i < rids.size(); i++) {
		godot::RID rid = rids[i];
		if (fi.LoadFontFromRID(rid, false, Rml::Style::FontWeight::Auto))
			any_ok = true;
	}

	if (any_ok) {
		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Font resource loaded: ") + font->get_font_name());
	} else {
		godot::UtilityFunctions::push_error("[RmlUi] Failed to load font resource");
	}
	return any_ok;
}

bool RmlContext::load_font_resource_ex(const godot::Ref<godot::Font>& font,
	const godot::String& family, int weight, bool fallback) {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load font — RmlUI not initialized");
		return false;
	}
	if (!font.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load font — null resource");
		return false;
	}

	auto& fi = manager->get_font_interface();
	godot::TypedArray<godot::RID> rids = font->get_rids();
	if (rids.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Font resource has no TextServer RIDs");
		return false;
	}

	Rml::String rml_family = family.is_empty()
		? Rml::String() : Rml::String(family.utf8().get_data());
	auto rml_weight = (weight > 0 && weight <= 1000)
		? static_cast<Rml::Style::FontWeight>(weight)
		: Rml::Style::FontWeight::Auto;

	bool any_ok = false;
	for (int i = 0; i < rids.size(); i++) {
		godot::RID rid = rids[i];
		if (fi.LoadFontFromRID(rid, fallback, rml_weight, rml_family))
			any_ok = true;
	}

	godot::String display_name = family.is_empty() ? font->get_font_name() : family;
	if (any_ok) {
		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Font resource loaded: ") + display_name);
	} else {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to load font resource: ") + display_name);
	}
	return any_ok;
}

void RmlContext::set_generic_family(const godot::String& generic_name, const godot::String& family_name) {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (!manager || !manager->is_initialized()) return;
	manager->get_font_interface().set_generic_family(
		Rml::String(generic_name.utf8().get_data()),
		Rml::String(family_name.utf8().get_data()));
}

godot::String RmlContext::get_generic_family(const godot::String& generic_name) const {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (!manager || !manager->is_initialized()) return {};
	Rml::String result = manager->get_font_interface().get_generic_family(
		Rml::String(generic_name.utf8().get_data()));
	return godot::String(result.c_str());
}

// --- Private: Context lifecycle ---

void RmlContext::set_text_render_mode(int mode) {
	_text_render_mode = mode;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_text_render_mode(mode);
		_render_dirty = true;
	}
}

void RmlContext::set_font_hinting(int hinting) {
	_font_hinting = hinting;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_hinting(hinting);
		_render_dirty = true;
	}
}

void RmlContext::set_font_antialiasing(int antialiasing) {
	_font_antialiasing = antialiasing;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_font_antialiasing(antialiasing);
		_render_dirty = true;
	}
}

void RmlContext::set_font_subpixel(int subpixel) {
	_font_subpixel = subpixel;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_subpixel_positioning(subpixel);
		_render_dirty = true;
	}
}

void RmlContext::set_font_oversampling(float oversampling) {
	_font_oversampling = oversampling;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_font_oversampling(oversampling);
		_render_dirty = true;
	}
}

void RmlContext::set_font_pixel_snap(bool snap) {
	_font_pixel_snap = snap;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_pixel_snap(snap);
		_render_dirty = true;
	}
}

void RmlContext::set_font_layout_mode(int mode) {
	_font_layout_mode = mode;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_layout_mode(mode);
		reload_all_documents();
		_render_dirty = true;
	}
}

bool RmlContext::inject_stylesheet(const godot::String& rcss_string) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot inject stylesheet — context not initialized");
		return false;
	}

	Rml::String rcss(rcss_string.utf8().get_data());
	auto new_styles = Rml::Factory::InstanceStyleSheetString(rcss);
	if (!new_styles) {
		godot::UtilityFunctions::push_error("[RmlUi] Failed to parse injected stylesheet");
		return false;
	}

	int injected_count = 0;
	for (auto& ld : _loaded_documents) {
		if (ld.document == nullptr) continue;

		const Rml::StyleSheetContainer* existing = ld.document->GetStyleSheetContainer();
		if (existing != nullptr) {
			auto combined = existing->CombineStyleSheetContainer(*new_styles);
			ld.document->SetStyleSheetContainer(std::move(combined));
		} else {
			ld.document->SetStyleSheetContainer(new_styles);
		}
		injected_count++;
	}

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Stylesheet injected into ") +
		godot::String::num_int64(injected_count) + godot::String(" document(s)"));
	return injected_count > 0;
}

bool RmlContext::unload_document(const godot::String& path) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot unload document — context not initialized");
		return false;
	}

	std::string path_str(path.utf8().get_data());

	auto it = std::find_if(_loaded_documents.begin(), _loaded_documents.end(),
		[&](const LoadedDocument& ld) { return ld.path == path_str; });

	if (it == _loaded_documents.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Document not tracked for unload: ") + path);
		return false;
	}

	if (it->document != nullptr) {
		_rml_context->UnloadDocument(it->document);
	}

	_loaded_documents.erase(it);

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Document unloaded: ") + path);
	return true;
}

} // namespace RmlGodot
