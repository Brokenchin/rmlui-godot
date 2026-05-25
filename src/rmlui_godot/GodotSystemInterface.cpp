#include "GodotSystemInterface.hpp"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

double GodotSystemInterface::GetElapsedTime() {
	return godot::Time::get_singleton()->get_ticks_msec() / 1000.0;
}

bool GodotSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message) {
	godot::String msg = godot::String("[RmlUi] ") + godot::String(message.c_str());
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
	return true;
}

void GodotSystemInterface::SetClipboardText(const Rml::String& text) {
	godot::DisplayServer::get_singleton()->clipboard_set(godot::String(text.c_str()));
}

void GodotSystemInterface::GetClipboardText(Rml::String& text) {
	godot::String clip = godot::DisplayServer::get_singleton()->clipboard_get();
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
