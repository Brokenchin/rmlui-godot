#include "RmlManager.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

static const char* k_builtin_default_rcss = R"rcss(
*, *::before, *::after {
	box-sizing: border-box;
}
body {
	display: block;
	overflow: hidden auto;
	font-size: 16px;
	line-height: 1.4;
	color: #e0e0e0;
}
div, p, h1, h2, h3, h4, h5, h6,
header, footer, section, nav,
blockquote, pre, form, fieldset {
	display: block;
}
em, i       { font-style: italic; }
strong, b   { font-weight: bold; }
h1 { font-size: 2em;    font-weight: bold; margin: 0.67em 0; }
h2 { font-size: 1.5em;  font-weight: bold; margin: 0.83em 0; }
h3 { font-size: 1.17em; font-weight: bold; margin: 1em 0; }
h4 { font-size: 1em;    font-weight: bold; margin: 1.33em 0; }
h5 { font-size: 0.83em; font-weight: bold; margin: 1.67em 0; }
h6 { font-size: 0.67em; font-weight: bold; margin: 2.33em 0; }
p { margin: 1em 0; }
ul, ol { padding-left: 2em; margin: 1em 0; }
table      { box-sizing: border-box; display: table; }
tr         { box-sizing: border-box; display: table-row; }
td, th     { box-sizing: border-box; display: table-cell; }
col        { box-sizing: border-box; display: table-column; }
colgroup   { display: table-column-group; }
thead, tbody, tfoot { display: table-row-group; }
th         { font-weight: bold; text-align: center; }
select     { text-align: left; }
tabset tabs { display: block; }
scrollbarvertical            { width: 12px; }
scrollbarvertical slidertrack   { background: #2a2a2a; }
scrollbarvertical sliderbar     { width: 12px; min-height: 20px; background: #555; }
scrollbarvertical sliderbar:hover  { background: #777; }
scrollbarvertical sliderbar:active { background: #999; }
scrollbarhorizontal          { height: 12px; }
scrollbarhorizontal slidertrack   { background: #2a2a2a; }
scrollbarhorizontal sliderbar     { height: 12px; min-width: 20px; background: #555; }
scrollbarhorizontal sliderbar:hover  { background: #777; }
scrollbarhorizontal sliderbar:active { background: #999; }
)rcss";

RmlManager* RmlManager::_singleton = nullptr;

RmlManager* RmlManager::get_singleton() {
	return _singleton;
}

RmlManager::RmlManager() {
	_singleton = this;
	_default_rcss = k_builtin_default_rcss;
}

RmlManager::~RmlManager() {
	_shutdown_rmlui();
	_singleton = nullptr;
}

void RmlManager::ensure_initialized() {
	_initialize_rmlui();
}

void RmlManager::on_context_created() {
	_context_count++;
}

void RmlManager::on_context_destroyed() {
	_context_count--;
}

// --- Global font management ---

bool RmlManager::load_font(const godot::String& path) {
	if (!_rmlui_initialized) {
		godot::UtilityFunctions::push_error("[RmlManager] Cannot load font — RmlUI not initialized");
		return false;
	}

	if (path.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlManager] Cannot load font — path is empty");
		return false;
	}

	Rml::String rml_path(path.utf8().get_data());
	bool ok = Rml::LoadFontFace(rml_path);
	if (ok) {
		_loaded_fonts.push_back(std::string(path.utf8().get_data()));
		godot::UtilityFunctions::print(godot::String("[RmlManager] Font loaded: ") + path);
	} else {
		godot::UtilityFunctions::push_error(godot::String("[RmlManager] Failed to load font: ") + path);
	}
	return ok;
}

godot::Array RmlManager::get_loaded_fonts() const {
	godot::Array result;
	for (const auto& f : _loaded_fonts) {
		result.append(godot::String(f.c_str()));
	}
	return result;
}

// --- Global texture cache ---

bool RmlManager::register_texture(const godot::String& name, const godot::Ref<godot::Texture2D>& texture) {
	if (name.is_empty() || texture.is_null()) {
		godot::UtilityFunctions::push_warning("[RmlManager] register_texture: invalid name or null texture");
		return false;
	}

	std::string key(name.utf8().get_data());
	_global_textures[key] = texture;
	godot::UtilityFunctions::print(godot::String("[RmlManager] Texture registered: ") + name);
	return true;
}

bool RmlManager::unregister_texture(const godot::String& name) {
	std::string key(name.utf8().get_data());
	auto it = _global_textures.find(key);
	if (it == _global_textures.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlManager] Texture not found for unregister: ") + name);
		return false;
	}
	_global_textures.erase(it);
	godot::UtilityFunctions::print(godot::String("[RmlManager] Texture unregistered: ") + name);
	return true;
}

godot::Ref<godot::Texture2D> RmlManager::get_texture(const godot::String& name) const {
	std::string key(name.utf8().get_data());
	auto it = _global_textures.find(key);
	if (it != _global_textures.end()) {
		return it->second;
	}
	return {};
}

bool RmlManager::has_texture(const godot::String& name) const {
	std::string key(name.utf8().get_data());
	return _global_textures.find(key) != _global_textures.end();
}

// --- Info ---

godot::Dictionary RmlManager::get_info() const {
	godot::Dictionary info;
	info["initialized"] = _rmlui_initialized;
	info["context_count"] = _context_count;
	info["loaded_fonts"] = static_cast<int>(_loaded_fonts.size());
	info["global_textures"] = static_cast<int>(_global_textures.size());
	info["registered_tags"] = static_cast<int>(_registered_tags.size());
	info["instancer_registered"] = _instancer_registered;
	info["array_type_registered"] = _array_type_registered;
	return info;
}

// --- Default RCSS ---

void RmlManager::set_default_rcss(const godot::String& rcss) {
	_default_rcss = Rml::String(rcss.utf8().get_data());
	_default_sheet.reset();
	_default_sheet_dirty = true;
}

godot::String RmlManager::get_default_rcss() const {
	return godot::String(_default_rcss.c_str());
}

void RmlManager::set_default_rcss_enabled(bool enabled) {
	_default_rcss_enabled = enabled;
}

Rml::SharedPtr<Rml::StyleSheetContainer> RmlManager::get_default_sheet() {
	if (!_default_rcss_enabled || _default_rcss.empty())
		return nullptr;

	if (_default_sheet_dirty || !_default_sheet) {
		_default_sheet = Rml::Factory::InstanceStyleSheetString(_default_rcss);
		_default_sheet_dirty = false;

		if (!_default_sheet) {
			godot::UtilityFunctions::push_error(
				"[RmlManager] Failed to parse default RCSS");
		}
	}
	return _default_sheet;
}

// --- Private: RmlUI lifecycle ---

void RmlManager::_initialize_rmlui() {
	if (_rmlui_initialized) return;

	Rml::SetSystemInterface(&_system_interface);
	Rml::SetFileInterface(&_file_interface);
	Rml::SetFontEngineInterface(&_font_interface);

	if (!Rml::Initialise()) {
		godot::UtilityFunctions::push_error("[RmlManager] Rml::Initialise() failed");
		return;
	}

	if (!_instancer_registered) {
		Rml::Factory::RegisterEventListenerInstancer(&_event_listener_instancer);
		_instancer_registered = true;
	}

	_rmlui_initialized = true;
	godot::UtilityFunctions::print("[RmlManager] RmlUI initialized");
}

void RmlManager::_shutdown_rmlui() {
	if (!_rmlui_initialized) return;

	_font_interface.ReleaseFontResources();
	_default_sheet.reset();
	_default_sheet_dirty = true;
	_global_textures.clear();
	_loaded_fonts.clear();
	_registered_tags.clear();

	Rml::Shutdown();
	_rmlui_initialized = false;
	_instancer_registered = false;
	_array_type_registered = false;
	godot::UtilityFunctions::print("[RmlManager] RmlUI shutdown");
}

} // namespace RmlGodot
