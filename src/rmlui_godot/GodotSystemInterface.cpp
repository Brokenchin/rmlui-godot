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

namespace {
// Per-element environment noise RmlUi emits in the editor. Fonts, data models and
// decorator shaders are all runtime/script-bound — registered from GDScript, which
// never runs in the editor — so a document re-emits these for EVERY affected
// element. The preview panel and diagnostics reload the whole document on every
// keystroke, so an unguarded push_warning here freezes live editing (issue #29:
// a missing font face alone produced ~21k warnings per reload). The editor's own
// diagnostics already classify exactly these as non-errors (rml_diagnostics.gd),
// so dropping them in the editor loses nothing actionable.
bool is_editor_env_noise(const Rml::String& message) {
	return message.find("No font face defined") != Rml::String::npos
		|| message.find("Could not locate data model") != Rml::String::npos
		|| message.find("Could not add data-") != Rml::String::npos
		|| message.find("Could not generate decorator element data") != Rml::String::npos;
}
} // namespace

bool GodotSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
	auto* engine = godot::Engine::get_singleton();
	const bool in_editor = engine != nullptr && engine->is_editor_hint();
	auto* manager = RmlManager::get_singleton();

	// Editor log-storm guard (issue #29). Both checks run BEFORE the muted/console
	// branches so they bound the rml_log signal volume too (diagnostics validates
	// muted and would otherwise emit one signal per element).
	if (in_editor) {
		// 1. Drop the known per-element environment noise outright.
		if (is_editor_env_noise(message)) {
			return true;
		}
		// 2. Backstop for any OTHER warning that turns out to be per-element: cap
		//    the burst so an unknown storm can't freeze editing either. Errors and
		//    asserts always pass through. The window is shared across all editor
		//    contexts (preview + diagnostics + 2D viewport), which is intended —
		//    a single keystroke reloads several of them at once.
		if (type != Rml::Log::LT_ERROR && type != Rml::Log::LT_ASSERT) {
			static double s_window_start = 0.0;
			static int s_count = 0;
			static bool s_notified = false;
			auto* time = godot::Time::get_singleton();
			const double now = time != nullptr ? static_cast<double>(time->get_ticks_msec()) : 0.0;
			if (now - s_window_start > 200.0) {
				s_window_start = now;
				s_count = 0;
				s_notified = false;
			}
			if (++s_count > 25) {
				if (!s_notified) {
					s_notified = true;
					godot::UtilityFunctions::print(
						"[RmlUi] (editor) suppressing a burst of warnings while editing — "
						"run/preview the document at runtime for the full log");
				}
				return true;
			}
		}
	}

	godot::String msg = godot::String("[RmlUi] ") + godot::String(message.c_str());

	// Console muted (editor diagnostics validating a buffer): skip Godot's
	// console entirely but keep the recent-log/signal forwarding below.
	if (manager != nullptr && manager->is_console_log_muted()) {
		manager->notify_log(static_cast<int>(type), godot::String(message.c_str()));
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
	if (manager != nullptr) {
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
