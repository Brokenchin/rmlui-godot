#include "GodotFileInterface.hpp"

#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

Rml::FileHandle GodotFileInterface::Open(const Rml::String& path) {
	godot::String gd_path = godot::String(path.c_str());

	// Default to res:// if no scheme is present.
	if (!gd_path.begins_with("res://") && !gd_path.begins_with("user://") && !gd_path.begins_with("/")) {
		gd_path = godot::String("res://") + gd_path;
	}

	godot::Ref<godot::FileAccess> file = godot::FileAccess::open(gd_path, godot::FileAccess::READ);
	if (!file.is_valid()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] FileInterface::Open failed: ") + gd_path);
		return 0;
	}

	uintptr_t handle = _next_handle++;
	_open_files[handle] = file;
	return handle;
}

void GodotFileInterface::Close(Rml::FileHandle file) {
	auto it = _open_files.find(file);
	if (it != _open_files.end()) {
		_open_files.erase(it);
	}
}

size_t GodotFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file) {
	auto it = _open_files.find(file);
	if (it == _open_files.end()) return 0;

	godot::PackedByteArray data = it->second->get_buffer(static_cast<int64_t>(size));
	size_t bytes_read = static_cast<size_t>(data.size());
	if (bytes_read > 0) {
		memcpy(buffer, data.ptr(), bytes_read);
	}
	return bytes_read;
}

bool GodotFileInterface::Seek(Rml::FileHandle file, long offset, int origin) {
	auto it = _open_files.find(file);
	if (it == _open_files.end()) return false;

	auto& fa = it->second;
	uint64_t length = fa->get_length();

	uint64_t target = 0;
	switch (origin) {
		case SEEK_SET: target = static_cast<uint64_t>(offset); break;
		case SEEK_CUR: target = fa->get_position() + static_cast<uint64_t>(offset); break;
		case SEEK_END: target = length + static_cast<uint64_t>(offset); break;
		default: return false;
	}

	fa->seek(target);
	return true;
}

size_t GodotFileInterface::Tell(Rml::FileHandle file) {
	auto it = _open_files.find(file);
	if (it == _open_files.end()) return 0;
	return static_cast<size_t>(it->second->get_position());
}

size_t GodotFileInterface::Length(Rml::FileHandle file) {
	auto it = _open_files.find(file);
	if (it == _open_files.end()) return 0;
	return static_cast<size_t>(it->second->get_length());
}

} // namespace RmlGodot
