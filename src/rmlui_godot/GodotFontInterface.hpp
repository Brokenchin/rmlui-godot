#pragma once

#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/FontMetrics.h>
#include <RmlUi/Core/CallbackTexture.h>

#include <godot_cpp/variant/rid.hpp>

#include <memory>
#include <unordered_map>
#include <vector>
#include <set>

namespace RmlGodot {

class GodotFontInterface final : public Rml::FontEngineInterface {
public:
	bool LoadFontFace(const Rml::String& file_name, int face_index, bool fallback_face, Rml::Style::FontWeight weight) override;
	bool LoadFontFace(Rml::Span<const Rml::byte> data, int face_index, const Rml::String& family,
		Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face) override;
	Rml::FontFaceHandle GetFontFaceHandle(const Rml::String& family, Rml::Style::FontStyle style,
		Rml::Style::FontWeight weight, int size) override;
	Rml::FontEffectsHandle PrepareFontEffects(Rml::FontFaceHandle handle, const Rml::FontEffectList& font_effects) override;
	const Rml::FontMetrics& GetFontMetrics(Rml::FontFaceHandle handle) override;
	int GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
		const Rml::TextShapingContext& text_shaping_context, Rml::Character prior_character) override;
	int GenerateString(Rml::RenderManager& render_manager, Rml::FontFaceHandle face_handle,
		Rml::FontEffectsHandle font_effects_handle, Rml::StringView string, Rml::Vector2f position,
		Rml::ColourbPremultiplied colour, float opacity, const Rml::TextShapingContext& text_shaping_context,
		Rml::TexturedMeshList& mesh_list) override;
	int GetVersion(Rml::FontFaceHandle handle) override;

	void ReleaseFontResources();
	void ReleaseTexturesForRenderManager(Rml::RenderManager* rm);

private:
	struct LoadedFont {
		godot::RID font_rid;
		Rml::String family;
		Rml::Style::FontStyle style = Rml::Style::FontStyle::Normal;
		Rml::Style::FontWeight weight = Rml::Style::FontWeight::Normal;
		bool is_fallback = false;
	};

	struct GlyphData {
		int texture_page = -1;
		Rml::Vector2f origin;
		Rml::Vector2f dimensions;
		Rml::Vector2f uv_min;
		Rml::Vector2f uv_max;
		float advance = 0;
		bool has_geometry = false;
	};

	struct FontFace {
		int loaded_font_index = -1;
		int size = 0;
		Rml::FontMetrics metrics{};
		int version = 0;
		std::unordered_map<uint32_t, GlyphData> glyph_cache;
		std::unordered_map<int, std::unique_ptr<Rml::CallbackTextureSource>> atlas_textures;
		std::set<int> dirty_pages;
	};

	std::vector<LoadedFont> _loaded_fonts;
	std::vector<std::unique_ptr<FontFace>> _faces;
	int _fallback_font_index = -1;

	int _find_font(const Rml::String& family, Rml::Style::FontStyle style, Rml::Style::FontWeight weight) const;
	bool _register_font(godot::RID font_rid, const Rml::String& family_override,
		Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face);
	const GlyphData& _ensure_glyph(FontFace& face, uint32_t codepoint);
	void _rebuild_dirty_atlases(FontFace& face);
};

} // namespace RmlGodot
