#include "GodotFontInterface.hpp"

#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/TextShapingContext.h>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

static godot::Ref<godot::TextServer> get_text_server() {
	return godot::TextServerManager::get_singleton()->get_primary_interface();
}

static Rml::String to_lower(const Rml::String& s) {
	Rml::String result = s;
	for (auto& c : result)
		if (c >= 'A' && c <= 'Z') c += 32;
	return result;
}

bool GodotFontInterface::LoadFontFace(const Rml::String& file_name, int /*face_index*/,
	bool fallback_face, Rml::Style::FontWeight weight) {

	godot::String path = godot::String(file_name.c_str());
	if (!path.begins_with("res://") && !path.begins_with("user://"))
		path = godot::String("res://") + path;

	godot::Ref<godot::FileAccess> f = godot::FileAccess::open(path, godot::FileAccess::READ);
	if (!f.is_valid()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi Font] Cannot open font file: ") + path);
		return false;
	}

	godot::PackedByteArray data = f->get_buffer(f->get_length());
	f->close();

	godot::Ref<godot::TextServer> ts = get_text_server();
	godot::RID font_rid = ts->create_font();
	ts->font_set_data(font_rid, data);

	Rml::String family(ts->font_get_name(font_rid).utf8().get_data());

	Rml::Style::FontStyle style = Rml::Style::FontStyle::Normal;
	if (static_cast<int64_t>(ts->font_get_style(font_rid)) & godot::TextServer::FONT_ITALIC)
		style = Rml::Style::FontStyle::Italic;

	return _register_font(font_rid, family, style, weight, fallback_face);
}

bool GodotFontInterface::LoadFontFace(Rml::Span<const Rml::byte> data, int /*face_index*/,
	const Rml::String& family, Rml::Style::FontStyle style,
	Rml::Style::FontWeight weight, bool fallback_face) {

	godot::Ref<godot::TextServer> ts = get_text_server();
	godot::RID font_rid = ts->create_font();

	godot::PackedByteArray bytes;
	bytes.resize(static_cast<int64_t>(data.size()));
	memcpy(bytes.ptrw(), data.data(), data.size());
	ts->font_set_data(font_rid, bytes);

	return _register_font(font_rid, family, style, weight, fallback_face);
}

bool GodotFontInterface::_register_font(godot::RID font_rid, const Rml::String& family_override,
	Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face) {

	LoadedFont loaded;
	loaded.font_rid = font_rid;
	loaded.family = family_override;
	loaded.style = style;
	loaded.weight = weight;
	loaded.is_fallback = fallback_face;

	if (fallback_face)
		_fallback_font_index = static_cast<int>(_loaded_fonts.size());

	_loaded_fonts.push_back(std::move(loaded));

	godot::UtilityFunctions::print(
		godot::String("[RmlUi Font] Loaded: ") + godot::String(family_override.c_str()) +
		godot::String(" (weight=") + godot::String::num_int64(static_cast<int>(weight)) +
		godot::String(", fallback=") + godot::String(fallback_face ? "true" : "false") +
		godot::String(")"));

	return true;
}

int GodotFontInterface::_find_font(const Rml::String& family, Rml::Style::FontStyle style,
	Rml::Style::FontWeight weight) const {

	Rml::String family_lower = to_lower(family);
	int best_idx = -1;
	int best_score = -1;

	for (int i = 0; i < static_cast<int>(_loaded_fonts.size()); i++) {
		if (to_lower(_loaded_fonts[i].family) != family_lower)
			continue;

		int score = 0;
		if (_loaded_fonts[i].style == style) score += 2;
		if (_loaded_fonts[i].weight == weight) score += 1;

		if (score > best_score) {
			best_score = score;
			best_idx = i;
		}
	}
	return best_idx;
}

Rml::FontFaceHandle GodotFontInterface::GetFontFaceHandle(const Rml::String& family,
	Rml::Style::FontStyle style, Rml::Style::FontWeight weight, int size) {

	int font_idx = _find_font(family, style, weight);
	if (font_idx < 0) {
		font_idx = _fallback_font_index;
		if (font_idx < 0) return 0;
	}

	for (size_t i = 0; i < _faces.size(); i++) {
		if (_faces[i]->loaded_font_index == font_idx && _faces[i]->size == size)
			return static_cast<Rml::FontFaceHandle>(i + 1);
	}

	godot::Ref<godot::TextServer> ts = get_text_server();
	const auto& font = _loaded_fonts[font_idx];

	auto face = std::make_unique<FontFace>();
	face->loaded_font_index = font_idx;
	face->size = size;
	face->version = 0;

	float ascent = static_cast<float>(ts->font_get_ascent(font.font_rid, size));
	float descent = static_cast<float>(ts->font_get_descent(font.font_rid, size));
	face->metrics.size = size;
	face->metrics.ascent = ascent;
	face->metrics.descent = descent;
	float leading = std::round(static_cast<float>(size) * 0.2f);
	face->metrics.line_spacing = ascent + descent + leading;
	face->metrics.x_height = ascent * 0.5f;
	face->metrics.underline_position = static_cast<float>(ts->font_get_underline_position(font.font_rid, size));
	face->metrics.underline_thickness = static_cast<float>(ts->font_get_underline_thickness(font.font_rid, size));
	if (face->metrics.underline_thickness < 1.0f) face->metrics.underline_thickness = 1.0f;

	face->metrics.has_ellipsis = ts->font_has_char(font.font_rid, 0x2026);

	_faces.push_back(std::move(face));
	return static_cast<Rml::FontFaceHandle>(_faces.size());
}

Rml::FontEffectsHandle GodotFontInterface::PrepareFontEffects(Rml::FontFaceHandle /*handle*/,
	const Rml::FontEffectList& /*font_effects*/) {
	return 0;
}

const Rml::FontMetrics& GodotFontInterface::GetFontMetrics(Rml::FontFaceHandle handle) {
	return _faces[handle - 1]->metrics;
}

int GodotFontInterface::GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
	const Rml::TextShapingContext& text_shaping_context, Rml::Character prior_character) {

	FontFace& face = *_faces[handle - 1];
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();

	float width = 0;
	bool use_kerning = (text_shaping_context.font_kerning != Rml::Style::FontKerning::None);
	uint32_t prev_codepoint = static_cast<uint32_t>(prior_character);

	for (auto it = Rml::StringIteratorU8(string); it; ++it) {
		uint32_t codepoint = static_cast<uint32_t>(*it);
		const GlyphData& glyph = _ensure_glyph(face, codepoint);

		if (use_kerning && prev_codepoint != 0) {
			int64_t prev_glyph = ts->font_get_glyph_index(font.font_rid, face.size, prev_codepoint, 0);
			int64_t curr_glyph = ts->font_get_glyph_index(font.font_rid, face.size, codepoint, 0);
			godot::Vector2 kern = ts->font_get_kerning(font.font_rid, face.size,
				godot::Vector2i(static_cast<int>(prev_glyph), static_cast<int>(curr_glyph)));
			width += static_cast<float>(kern.x);
		}

		width += glyph.advance + text_shaping_context.letter_spacing;
		prev_codepoint = codepoint;
	}

	return Rml::Math::Max(static_cast<int>(width), 0);
}

int GodotFontInterface::GenerateString(Rml::RenderManager& render_manager,
	Rml::FontFaceHandle face_handle, Rml::FontEffectsHandle /*font_effects_handle*/,
	Rml::StringView string, Rml::Vector2f position, Rml::ColourbPremultiplied colour,
	float /*opacity*/, const Rml::TextShapingContext& text_shaping_context,
	Rml::TexturedMeshList& mesh_list) {

	FontFace& face = *_faces[face_handle - 1];
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();

	bool use_kerning = (text_shaping_context.font_kerning != Rml::Style::FontKerning::None);

	// Phase 1: ensure all glyphs are cached
	for (auto it = Rml::StringIteratorU8(string); it; ++it)
		_ensure_glyph(face, static_cast<uint32_t>(*it));

	// Phase 2: rebuild atlas textures for any dirty pages
	_rebuild_dirty_atlases(face);

	// Phase 3: collect unique texture pages
	std::set<int> used_pages;
	for (auto it = Rml::StringIteratorU8(string); it; ++it) {
		uint32_t cp = static_cast<uint32_t>(*it);
		auto git = face.glyph_cache.find(cp);
		if (git != face.glyph_cache.end() && git->second.has_geometry)
			used_pages.insert(git->second.texture_page);
	}

	if (used_pages.empty()) {
		// No visible glyphs — still compute width
		float width = 0;
		for (auto it = Rml::StringIteratorU8(string); it; ++it) {
			const GlyphData& g = face.glyph_cache[static_cast<uint32_t>(*it)];
			width += g.advance + text_shaping_context.letter_spacing;
		}
		return Rml::Math::Max(static_cast<int>(width), 0);
	}

	// Phase 4: assign mesh_list entries
	mesh_list.resize(static_cast<int>(used_pages.size()));
	std::unordered_map<int, int> page_to_mesh;
	int mesh_idx = 0;
	for (int page : used_pages) {
		page_to_mesh[page] = mesh_idx;
		auto tex_it = face.atlas_textures.find(page);
		if (tex_it != face.atlas_textures.end() && tex_it->second)
			mesh_list[mesh_idx].texture = tex_it->second->GetTexture(render_manager);
		mesh_idx++;
	}

	// Reserve vertex/index space
	size_t char_count = 0;
	for (auto it = Rml::StringIteratorU8(string); it; ++it) ++char_count;
	if (!mesh_list.empty()) {
		mesh_list[0].mesh.vertices.reserve(char_count * 4);
		mesh_list[0].mesh.indices.reserve(char_count * 6);
	}

	// Phase 5: generate geometry
	float cursor_x = 0;
	uint32_t prev_codepoint = 0;

	for (auto it = Rml::StringIteratorU8(string); it; ++it) {
		uint32_t codepoint = static_cast<uint32_t>(*it);
		const GlyphData& glyph = face.glyph_cache[codepoint];

		if (use_kerning && prev_codepoint != 0) {
			int64_t prev_gi = ts->font_get_glyph_index(font.font_rid, face.size, prev_codepoint, 0);
			int64_t curr_gi = ts->font_get_glyph_index(font.font_rid, face.size, codepoint, 0);
			godot::Vector2 kern = ts->font_get_kerning(font.font_rid, face.size,
				godot::Vector2i(static_cast<int>(prev_gi), static_cast<int>(curr_gi)));
			cursor_x += static_cast<float>(kern.x);
		}

		if (glyph.has_geometry) {
			int mi = page_to_mesh[glyph.texture_page];
			Rml::Vector2f glyph_pos(
				position.x + cursor_x + glyph.origin.x,
				position.y + glyph.origin.y);
			Rml::MeshUtilities::GenerateQuad(
				mesh_list[mi].mesh, glyph_pos, glyph.dimensions,
				colour, glyph.uv_min, glyph.uv_max);
		}

		cursor_x += glyph.advance + text_shaping_context.letter_spacing;
		prev_codepoint = codepoint;
	}

	return Rml::Math::Max(static_cast<int>(cursor_x), 0);
}

int GodotFontInterface::GetVersion(Rml::FontFaceHandle handle) {
	return _faces[handle - 1]->version;
}

// --- Private: glyph caching ---

const GodotFontInterface::GlyphData& GodotFontInterface::_ensure_glyph(FontFace& face, uint32_t codepoint) {
	auto it = face.glyph_cache.find(codepoint);
	if (it != face.glyph_cache.end())
		return it->second;

	godot::Ref<godot::TextServer> ts = get_text_server();
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Vector2i size_v(face.size, 0);

	int64_t glyph_index = ts->font_get_glyph_index(font.font_rid, face.size, codepoint, 0);

	GlyphData glyph;
	glyph.advance = static_cast<float>(ts->font_get_glyph_advance(font.font_rid, face.size, glyph_index).x);

	godot::Vector2 glyph_size = ts->font_get_glyph_size(font.font_rid, size_v, glyph_index);
	if (glyph_size.x < 1 || glyph_size.y < 1) {
		glyph.has_geometry = false;
		auto [inserted, _] = face.glyph_cache.emplace(codepoint, glyph);
		return inserted->second;
	}

	ts->font_render_glyph(font.font_rid, size_v, glyph_index);

	godot::Vector2 offset = ts->font_get_glyph_offset(font.font_rid, size_v, glyph_index);
	godot::Rect2 uv_rect = ts->font_get_glyph_uv_rect(font.font_rid, size_v, glyph_index);
	godot::Vector2 tex_size = ts->font_get_glyph_texture_size(font.font_rid, size_v, glyph_index);
	int64_t tex_idx = ts->font_get_glyph_texture_idx(font.font_rid, size_v, glyph_index);

	glyph.texture_page = static_cast<int>(tex_idx);
	glyph.origin = Rml::Vector2f(static_cast<float>(offset.x), static_cast<float>(offset.y));
	glyph.dimensions = Rml::Vector2f(static_cast<float>(glyph_size.x), static_cast<float>(glyph_size.y));
	glyph.has_geometry = true;

	if (tex_size.x > 0 && tex_size.y > 0) {
		glyph.uv_min.x = static_cast<float>(uv_rect.position.x / tex_size.x);
		glyph.uv_min.y = static_cast<float>(uv_rect.position.y / tex_size.y);
		glyph.uv_max.x = static_cast<float>((uv_rect.position.x + uv_rect.size.x) / tex_size.x);
		glyph.uv_max.y = static_cast<float>((uv_rect.position.y + uv_rect.size.y) / tex_size.y);
	}

	face.dirty_pages.insert(glyph.texture_page);
	auto [inserted, _] = face.glyph_cache.emplace(codepoint, glyph);
	return inserted->second;
}

// --- Private: atlas texture rebuild ---

void GodotFontInterface::_rebuild_dirty_atlases(FontFace& face) {
	if (face.dirty_pages.empty())
		return;

	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::RID font_rid = font.font_rid;
	godot::Vector2i size_v(face.size, 0);

	for (int page : face.dirty_pages) {
		int page_idx = page;

		Rml::CallbackTextureFunction callback = [font_rid, size_v, page_idx](
			const Rml::CallbackTextureInterface& tex_interface) -> bool {

			godot::Ref<godot::TextServer> ts = get_text_server();
			godot::Ref<godot::Image> atlas = ts->font_get_texture_image(font_rid, size_v, page_idx);
			if (!atlas.is_valid() || atlas->is_empty()) {
				godot::UtilityFunctions::push_warning(
					godot::String("[RmlUi Atlas] Empty/invalid atlas for page=") + godot::String::num_int64(page_idx));
				return false;
			}

			godot::Ref<godot::Image> img = godot::Image::create_from_data(
				atlas->get_width(), atlas->get_height(), false, atlas->get_format(), atlas->get_data());
			if (img->get_format() != godot::Image::FORMAT_RGBA8)
				img->convert(godot::Image::FORMAT_RGBA8);

			int w = img->get_width();
			int h = img->get_height();
			godot::PackedByteArray data = img->get_data();

			// Premultiply alpha
			uint8_t* px = data.ptrw();
			int count = w * h;
			for (int i = 0; i < count; i++) {
				uint8_t r = px[i * 4 + 0];
				uint8_t g = px[i * 4 + 1];
				uint8_t b = px[i * 4 + 2];
				uint8_t a = px[i * 4 + 3];
				px[i * 4 + 0] = static_cast<uint8_t>((r * a) / 255);
				px[i * 4 + 1] = static_cast<uint8_t>((g * a) / 255);
				px[i * 4 + 2] = static_cast<uint8_t>((b * a) / 255);
			}

			return tex_interface.GenerateTexture(
				Rml::Span<const Rml::byte>(data.ptr(), data.size()),
				Rml::Vector2i(w, h));
		};

		face.atlas_textures[page_idx] = std::make_unique<Rml::CallbackTextureSource>(std::move(callback));
	}

	face.dirty_pages.clear();
	face.version++;
}

void GodotFontInterface::ReleaseTexturesForRenderManager(Rml::RenderManager* rm) {
	for (auto& face : _faces) {
		for (auto& [page, tex_source] : face->atlas_textures) {
			if (tex_source)
				tex_source->ReleaseForRenderManager(rm);
		}
	}
}

void GodotFontInterface::ReleaseFontResources() {
	godot::Ref<godot::TextServer> ts = get_text_server();
	for (auto& font : _loaded_fonts) {
		if (font.font_rid.is_valid())
			ts->free_rid(font.font_rid);
	}
	_loaded_fonts.clear();
	_faces.clear();
	_fallback_font_index = -1;
}

} // namespace RmlGodot
