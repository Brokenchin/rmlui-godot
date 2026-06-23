#include "GodotSystemInterface.hpp"
#include "RmlManager.hpp"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

double GodotSystemInterface::GetElapsedTime() {
	auto* time = godot::Time::get_singleton();
	if (time == nullptr) return 0.0;
	return time->get_ticks_msec() / 1000.0;
}

bool GodotSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
	godot::String msg = godot::String("[RmlUi] ") + godot::String(message.c_str());

	// Console muted (editor diagnostics validating a buffer): skip Godot's
	// console entirely but keep the recent-log/signal forwarding below.
	auto* mute_manager = RmlManager::get_singleton();
	if (mute_manager != nullptr && mute_manager->is_console_log_muted()) {
		mute_manager->notify_log(static_cast<int>(type), godot::String(message.c_str()));
		return true;
	}

	// Data models are bound at runtime from script — inside the editor a
	// document referencing one is expected, not an error. Downgrade so the
	// editor console isn't spammed red on every selection/preview.
	auto* engine = godot::Engine::get_singleton();
	const bool in_editor = engine != nullptr && engine->is_editor_hint();
	if (type == Rml::Log::LT_ERROR && in_editor &&
		message.find("Could not locate data model") != Rml::String::npos) {
		type = Rml::Log::LT_WARNING;
	}

	// Editor-only: a decorator whose shader is registered from GDScript (which
	// never runs in the editor) fails to generate data for EVERY decorated
	// element, and RmlUi emits one warning per element. With per-keystroke
	// preview reloads this push_warning storm freezes editing (issue #29). The
	// root cause is already reported once via "No decorator shader registered
	// for ..." (GodotRenderInterface::CompileShader), so these per-element
	// follow-ups are redundant noise here — drop them entirely.
	if (in_editor && type == Rml::Log::LT_WARNING &&
		message.find("Could not generate decorator element data") != Rml::String::npos) {
		return true;
	}

	switch (type) {
		case Rml::Log::LT_ERROR:
		case Rml::Log::LT_ASSERT:
			godot::UtilityFunctions::push_error(msg);
			break;
		case Rml::Log::LT_WARNING:
			godot::UtilityFunctions::push_warning(msg);
			break;
		default:
			godot::UtilityFunctions::print(msg);
			break;
	}

	// Forward to RmlManager so tooling (editor preview panel, validators)
	// can subscribe via the "rml_log" signal.
	if (auto* manager = RmlManager::get_singleton()) {
		manager->notify_log(static_cast<int>(type), godot::String(message.c_str()));
	}
	return true;
}

void GodotSystemInterface::SetClipboardText(const Rml::String& text) {
	auto* ds = godot::DisplayServer::get_singleton();
	if (ds == nullptr) return;
	ds->clipboard_set(godot::String(text.c_str()));
}

void GodotSystemInterface::GetClipboardText(Rml::String& text) {
	auto* ds = godot::DisplayServer::get_singleton();
	if (ds == nullptr) { text.clear(); return; }
	godot::String clip = ds->clipboard_get();
	text = Rml::String(clip.utf8().get_data());
}

void GodotSystemInterface::JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path) {
	// If the path is already absolute (any scheme like res://, user://, texture://, or filesystem root), use it directly.
	if (path.find("://") != Rml::String::npos || path.substr(0, 1) == "/") {
		translated_path = path;
		return;
	}

	// Otherwise join relative to the document's directory.
	size_t pos = document_path.rfind('/');
	if (pos == Rml::String::npos)
		pos = document_path.rfind('\\');

	if (pos != Rml::String::npos)
		translated_path = document_path.substr(0, pos + 1) + path;
	else
		translated_path = path;
}

} // namespace RmlGodot
