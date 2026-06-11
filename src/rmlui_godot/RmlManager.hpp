#pragma once

#include "RmlGD.hpp"
#include "GodotSystemInterface.hpp"
#include "GodotFileInterface.hpp"
#include "GodotFontInterface.hpp"
#include "GodotEventListenerInstancer.hpp"
#include "GodotElementInstancer.hpp"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <RmlUi/Core/StyleSheetContainer.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace RmlGodot {

class RM_GD_CLASS(RmlManager, godot::Object, {

	godot::ClassDB::bind_method(godot::D_METHOD("load_font", "path"), &RmlManager::load_font);
	godot::ClassDB::bind_method(godot::D_METHOD("get_loaded_fonts"), &RmlManager::get_loaded_fonts);
	godot::ClassDB::bind_method(godot::D_METHOD("register_texture", "name", "texture"), &RmlManager::register_texture);
	godot::ClassDB::bind_method(godot::D_METHOD("unregister_texture", "name"), &RmlManager::unregister_texture);
	godot::ClassDB::bind_method(godot::D_METHOD("get_texture", "name"), &RmlManager::get_texture);
	godot::ClassDB::bind_method(godot::D_METHOD("has_texture", "name"), &RmlManager::has_texture);
	godot::ClassDB::bind_method(godot::D_METHOD("is_initialized"), &RmlManager::is_initialized);
	godot::ClassDB::bind_method(godot::D_METHOD("get_context_count"), &RmlManager::get_context_count);
	godot::ClassDB::bind_method(godot::D_METHOD("get_info"), &RmlManager::get_info);

	godot::ClassDB::bind_method(godot::D_METHOD("set_default_rcss", "rcss"), &RmlManager::set_default_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("get_default_rcss"), &RmlManager::get_default_rcss);
	godot::ClassDB::bind_method(godot::D_METHOD("set_default_rcss_enabled", "enabled"), &RmlManager::set_default_rcss_enabled);
	godot::ClassDB::bind_method(godot::D_METHOD("is_default_rcss_enabled"), &RmlManager::is_default_rcss_enabled);
	godot::ClassDB::bind_method(godot::D_METHOD("get_recent_log"), &RmlManager::get_recent_log);
	godot::ClassDB::bind_method(godot::D_METHOD("clear_recent_log"), &RmlManager::clear_recent_log);
	godot::ClassDB::bind_method(godot::D_METHOD("get_supported_rcss_properties"), &RmlManager::get_supported_rcss_properties);

	ADD_PROPERTY(godot::PropertyInfo(godot::Variant::BOOL, "default_rcss_enabled"), "set_default_rcss_enabled", "is_default_rcss_enabled");

	ADD_SIGNAL(godot::MethodInfo("rml_log",
		godot::PropertyInfo(godot::Variant::INT, "level"),
		godot::PropertyInfo(godot::Variant::STRING, "message")));

});

public:
	static RmlManager* get_singleton();

	RmlManager();
	~RmlManager() override;

	void ensure_initialized();
	void on_context_created();
	void on_context_destroyed();
	bool is_initialized() const { return _rmlui_initialized; }

	bool load_font(const godot::String& path);
	godot::Array get_loaded_fonts() const;

	bool register_texture(const godot::String& name, const godot::Ref<godot::Texture2D>& texture);
	bool unregister_texture(const godot::String& name);
	godot::Ref<godot::Texture2D> get_texture(const godot::String& name) const;
	bool has_texture(const godot::String& name) const;

	int get_context_count() const { return _context_count; }
	godot::Dictionary get_info() const;

	void set_default_rcss(const godot::String& rcss);
	godot::String get_default_rcss() const;
	void set_default_rcss_enabled(bool enabled);
	bool is_default_rcss_enabled() const { return _default_rcss_enabled; }
	Rml::SharedPtr<Rml::StyleSheetContainer> get_default_sheet();

	GodotSystemInterface& get_system_interface() { return _system_interface; }
	GodotFileInterface& get_file_interface() { return _file_interface; }
	GodotFontInterface& get_font_interface() { return _font_interface; }
	GodotEventListenerInstancer& get_event_listener_instancer() { return _event_listener_instancer; }
	GodotElementInstancer& get_element_instancer() { return _element_instancer; }

	// RmlUi log forwarding — GodotSystemInterface::LogMessage reports here so
	// editor tooling can subscribe via the "rml_log" signal. Levels are
	// Rml::Log::Type values (1=error, 2=assert, 3=warning, 4=info, 5=debug).
	void notify_log(int level, const godot::String& message);
	godot::Array get_recent_log() const { return _recent_log; }
	void clear_recent_log() { _recent_log.clear(); }

	// Every property + shorthand registered with RmlUi's stylesheet engine —
	// the authoritative source for editor autocomplete.
	godot::PackedStringArray get_supported_rcss_properties() const;

	bool is_instancer_registered() const { return _instancer_registered; }
	void set_instancer_registered(bool v) { _instancer_registered = v; }
	std::vector<std::string>& get_registered_tags() { return _registered_tags; }



private:
	static RmlManager* _singleton;

	GodotSystemInterface _system_interface;
	GodotFileInterface _file_interface;
	GodotFontInterface _font_interface;
	GodotEventListenerInstancer _event_listener_instancer;
	GodotElementInstancer _element_instancer;

	bool _rmlui_initialized = false;
	bool _instancer_registered = false;
	int _context_count = 0;

	std::vector<std::string> _registered_tags;
	std::vector<std::string> _loaded_fonts;
	std::unordered_map<std::string, godot::Ref<godot::Texture2D>> _global_textures;

	Rml::String _default_rcss;
	Rml::SharedPtr<Rml::StyleSheetContainer> _default_sheet;
	bool _default_rcss_enabled = true;
	bool _default_sheet_dirty = true;

	godot::Array _recent_log;

	void _initialize_rmlui();
	void _shutdown_rmlui();
};

} // namespace RmlGodot
