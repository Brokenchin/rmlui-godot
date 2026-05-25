#pragma once

#include <RmlUi/Core/FileInterface.h>
#include <godot_cpp/classes/file_access.hpp>
#include <unordered_map>

namespace RmlGodot {

class GodotFileInterface final : public Rml::FileInterface {
public:
	Rml::FileHandle Open(const Rml::String& path) override;
	void Close(Rml::FileHandle file) override;
	size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;
	size_t Length(Rml::FileHandle file) override;

private:
	std::unordered_map<uintptr_t, godot::Ref<godot::FileAccess>> _open_files;
	uintptr_t _next_handle = 1;
};

} // namespace RmlGodot
