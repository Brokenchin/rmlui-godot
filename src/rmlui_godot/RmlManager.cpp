#include "RmlManager.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

RmlManager* RmlManager::_singleton = nullptr;

RmlManager* RmlManager::get_singleton() {
	return _singleton;
}

RmlManager::RmlManager() {
	_singleton = this;
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
