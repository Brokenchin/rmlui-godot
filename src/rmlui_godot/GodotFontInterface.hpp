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
	enum class TextRenderMode : int {
		DEFAULT = 0,
		SUBPIXEL = 1,
		OVERSAMPLED = 2,
		HIGH_QUALITY = 3,
	};

	// How glyphs are laid out horizontally.
	enum class LayoutMode : int {
		// Fractional advance accumulated, each glyph floored independently when
		// pixel-snap is on. Sub-pixel gaps can collapse → occasional touching
		// letters, but crisp on the pixel grid.
		MANUAL = 0,
		// Like MANUAL but each glyph advance is rounded to a whole pixel, so
		// inter-glyph gaps are uniform (no 4/5/4/5 shimmer). Not metrically
		// identical to Godot but cleanest for nearest-filter pixel text.
		INTEGER_ADVANCE = 1,
		// Lay out via Godot's TextServer shaped-text pipeline: advances, kerning
		// and per-glyph offsets come straight from Godot for true parity.
		SHAPED = 2,
	};

	void set_text_render_mode(TextRenderMode mode);
	TextRenderMode get_text_render_mode() const { return _text_render_mode; }

	void set_layout_mode(int mode);
	int get_layout_mode() const { return static_cast<int>(_layout_mode); }

	// Granular font tuning (mirrors godot::TextServer enums; stored as int to
	// keep this header free of the TextServer include). Each setter re-applies
	// to all loaded fonts and invalidates the glyph caches so the change is
	// visible on the next draw.
	void set_hinting(int hinting);
	int get_hinting() const { return _hinting; }
	void set_font_antialiasing(int antialiasing);
	int get_font_antialiasing() const { return _antialiasing; }
	void set_subpixel_positioning(int subpixel);
	int get_subpixel_positioning() const { return _subpixel; }
	void set_font_oversampling(float oversampling);
	float get_font_oversampling() const { return _oversampling; }
	void set_pixel_snap(bool snap);
	bool get_pixel_snap() const { return _pixel_snap; }

	bool LoadFontFace(const Rml::String& file_name, int face_index, bool fallback_face, Rml::Style::FontWeight weight) override;
	bool LoadFontFace(Rml::Span<const Rml::byte> data, int face_index, const Rml::String& family,
		Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face) override;
	bool LoadFontFromRID(godot::RID font_rid, bool fallback_face, Rml::Style::FontWeight weight);
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
		bool externally_owned = false;
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
		std::unordered_map<uint32_t, GlyphData> glyph_cache;       // keyed by codepoint (MANUAL/INTEGER)
		std::unordered_map<uint32_t, GlyphData> glyph_index_cache; // keyed by glyph index, may include subpixel shift bits 27-28
		std::unordered_map<uint32_t, int64_t> codepoint_to_index;  // codepoint → raw glyph index (no shift bits)
		std::unordered_map<int, std::unique_ptr<Rml::CallbackTextureSource>> atlas_textures;
		std::set<int> dirty_pages;
	};

	std::vector<LoadedFont> _loaded_fonts;
	std::vector<std::unique_ptr<FontFace>> _faces;
	int _fallback_font_index = -1;
	TextRenderMode _text_render_mode = TextRenderMode::DEFAULT;
	LayoutMode _layout_mode = LayoutMode::MANUAL;

	// Defaults chosen to match Godot's default FontFile import + Label render:
	// HINTING_LIGHT(1), FONT_ANTIALIASING_GRAY(1), SUBPIXEL_POSITIONING_DISABLED(0).
	int _hinting = 1;
	int _antialiasing = 1;
	int _subpixel = 0;
	float _oversampling = 0.0f;
	bool _pixel_snap = true;

	int _find_font(const Rml::String& family, Rml::Style::FontStyle style, Rml::Style::FontWeight weight) const;
	bool _register_font(godot::RID font_rid, const Rml::String& family_override,
		Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face);
	GlyphData _build_glyph_data(FontFace& face, int64_t glyph_index);
	const GlyphData& _ensure_glyph(FontFace& face, uint32_t codepoint);
	const GlyphData& _ensure_glyph_index(FontFace& face, int64_t glyph_index);
	// Builds + shapes a TextServer shaped-text RID for the string at the
	// (oversampled) render size. Caller owns the RID and must free_rid it.
	godot::RID _shape_string(const FontFace& face, Rml::StringView string) const;
	int _generate_shaped(Rml::RenderManager& render_manager, FontFace& face, Rml::StringView string,
		Rml::Vector2f position, Rml::ColourbPremultiplied colour,
		const Rml::TextShapingContext& text_shaping_context, Rml::TexturedMeshList& mesh_list);
	void _rebuild_dirty_atlases(FontFace& face);
	void _apply_font_settings(godot::RID font_rid) const;
	void _invalidate_all_caches();
	int _effective_subpixel_mode(const LoadedFont& font, int render_size) const;
	static int _compute_subpixel_shift(int subpixel_mode, float pen_x);

	// Oversampling factor (>=1.0). When >1, glyphs are rasterized at
	// size*factor and the quad is scaled down by 1/factor, mirroring Godot's
	// font_draw_glyph. A value of 0.0 ("auto") means no manual oversampling.
	float _oversample_factor() const { return _oversampling > 1.0f ? _oversampling : 1.0f; }
	int _render_size(int logical) const {
		float f = _oversample_factor();
		return f > 1.0f ? static_cast<int>(logical * f + 0.5f) : logical;
	}
};

} // namespace RmlGodot
