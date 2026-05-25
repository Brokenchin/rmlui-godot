#pragma once

#include <RmlUi/Core/SystemInterface.h>

namespace RmlGodot {

class GodotSystemInterface final : public Rml::SystemInterface {
public:
	double GetElapsedTime() override;
	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
	void SetClipboardText(const Rml::String& text) override;
	void GetClipboardText(Rml::String& text) override;
	void JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path) override;
};

} // namespace RmlGodot
