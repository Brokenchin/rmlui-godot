#include "GodotFontInterface.hpp"

#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/TextShapingContext.h>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
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
	if (path.begins_with("uid://")) {
		auto* uid_mgr = godot::ResourceUID::get_singleton();
		int64_t uid = uid_mgr->text_to_id(path);
		if (uid_mgr->has_id(uid))
			path = uid_mgr->get_id_path(uid);
	}
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
	_apply_font_settings(font_rid);

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
	_apply_font_settings(font_rid);

	return _register_font(font_rid, family, style, weight, fallback_face);
}

bool GodotFontInterface::LoadFontFromRID(godot::RID font_rid, bool fallback_face,
	Rml::Style::FontWeight weight) {

	if (!font_rid.is_valid()) return false;

	godot::Ref<godot::TextServer> ts = get_text_server();

	Rml::String family(ts->font_get_name(font_rid).utf8().get_data());

	Rml::Style::FontStyle style = Rml::Style::FontStyle::Normal;
	if (static_cast<int64_t>(ts->font_get_style(font_rid)) & godot::TextServer::FONT_ITALIC)
		style = Rml::Style::FontStyle::Italic;

	if (!_register_font(font_rid, family, style, weight, fallback_face))
		return false;
	_loaded_fonts.back().externally_owned = true;
	return true;
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
	static const Rml::FontMetrics s_empty{};
	if (handle == 0 || handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return s_empty;
	return _faces[handle - 1]->metrics;
}

int GodotFontInterface::GetStringWidth(Rml::FontFaceHandle handle, Rml::StringView string,
	const Rml::TextShapingContext& text_shaping_context, Rml::Character prior_character) {

	if (handle == 0 || handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return 0;
	FontFace& face = *_faces[handle - 1];
	if (face.loaded_font_index < 0 || face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return 0;
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null()) return 0;

	const float os = _oversample_factor();

	if (_layout_mode == LayoutMode::SHAPED) {
		// Advances, kerning and ligatures all come from Godot's shaper. Width
		// must match GenerateString, which adds letter_spacing per glyph.
		godot::RID shaped = _shape_string(face, string);
		if (!shaped.is_valid()) return 0;
		float width = static_cast<float>(ts->shaped_text_get_width(shaped)) / os;
		width += text_shaping_context.letter_spacing *
			static_cast<float>(ts->shaped_text_get_glyph_count(shaped));
		ts->free_rid(shaped);
		return Rml::Math::Max(static_cast<int>(width), 0);
	}

	float width = 0;
	float adv_rem = 0;
	bool use_kerning = (text_shaping_context.font_kerning != Rml::Style::FontKerning::None);
	uint32_t prev_codepoint = static_cast<uint32_t>(prior_character);
	const bool integer_advance = (_layout_mode == LayoutMode::INTEGER_ADVANCE);

	for (auto it = Rml::StringIteratorU8(string); it; ++it) {
		uint32_t codepoint = static_cast<uint32_t>(*it);
		const GlyphData& glyph = _ensure_glyph(face, codepoint);

		float kern_adj = 0;
		if (use_kerning && prev_codepoint != 0) {
			const int rsize = _render_size(face.size);
			int64_t prev_glyph = ts->font_get_glyph_index(font.font_rid, rsize, prev_codepoint, 0);
			int64_t curr_glyph = ts->font_get_glyph_index(font.font_rid, rsize, codepoint, 0);
			godot::Vector2 kern = ts->font_get_kerning(font.font_rid, rsize,
				godot::Vector2i(static_cast<int>(prev_glyph), static_cast<int>(curr_glyph)));
			kern_adj = static_cast<float>(kern.x) / os;
		}

		float raw = kern_adj + glyph.advance + text_shaping_context.letter_spacing;
		if (integer_advance) {
			raw += adv_rem;
			float rounded = Rml::Math::Round(raw);
			adv_rem = raw - rounded;
			width += rounded;
		} else {
			width += raw;
		}
		prev_codepoint = codepoint;
	}

	return Rml::Math::Max(static_cast<int>(width), 0);
}

int GodotFontInterface::GenerateString(Rml::RenderManager& render_manager,
	Rml::FontFaceHandle face_handle, Rml::FontEffectsHandle /*font_effects_handle*/,
	Rml::StringView string, Rml::Vector2f position, Rml::ColourbPremultiplied colour,
	float /*opacity*/, const Rml::TextShapingContext& text_shaping_context,
	Rml::TexturedMeshList& mesh_list) {

	if (face_handle == 0 || face_handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return 0;
	FontFace& face = *_faces[face_handle - 1];
	if (face.loaded_font_index < 0 || face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return 0;
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null()) return 0;

	if (_layout_mode == LayoutMode::SHAPED)
		return _generate_shaped(render_manager, face, string, position, colour, text_shaping_context, mesh_list);

	const bool integer_advance = (_layout_mode == LayoutMode::INTEGER_ADVANCE);
	bool use_kerning = (text_shaping_context.font_kerning != Rml::Style::FontKerning::None);
	const float os = _oversample_factor();
	const int rsize = _render_size(face.size);
	const int subpx_mode = _effective_subpixel_mode(font, rsize);

	// Phase 1: ensure base glyphs are cached (for advances + triggers font_render_glyph
	// which pre-renders all subpixel variants into the TextServer atlas).
	for (auto it = Rml::StringIteratorU8(string); it; ++it)
		_ensure_glyph(face, static_cast<uint32_t>(*it));

	// Phase 2: pre-compute positions and ensure subpixel-shifted variants.
	struct GlyphEntry { uint32_t composite_key; float kern_adj; float advance; };
	std::vector<GlyphEntry> entries;
	{
		float cursor_x = 0;
		float adv_rem_pre = 0;
		uint32_t prev_cp = 0;
		for (auto it = Rml::StringIteratorU8(string); it; ++it) {
			uint32_t cp = static_cast<uint32_t>(*it);
			const GlyphData& base = face.glyph_cache[cp];

			float kern_adj = 0;
			if (use_kerning && prev_cp != 0) {
				int64_t prev_gi = face.codepoint_to_index[prev_cp];
				int64_t curr_gi = face.codepoint_to_index[cp];
				godot::Vector2 kern = ts->font_get_kerning(font.font_rid, rsize,
					godot::Vector2i(static_cast<int>(prev_gi), static_cast<int>(curr_gi)));
				kern_adj = static_cast<float>(kern.x) / os;
			}

			float pen_x = position.x + cursor_x + kern_adj;
			int shift = _compute_subpixel_shift(subpx_mode, pen_x);
			int64_t raw_idx = face.codepoint_to_index[cp];
			int64_t composite = raw_idx | (static_cast<int64_t>(shift) << 27);
			uint32_t key = static_cast<uint32_t>(composite);

			_ensure_glyph_index(face, composite);

			entries.push_back({key, kern_adj, base.advance});

			float raw = kern_adj + base.advance + text_shaping_context.letter_spacing;
			if (integer_advance) {
				raw += adv_rem_pre;
				float rounded = Rml::Math::Round(raw);
				adv_rem_pre = raw - rounded;
				cursor_x += rounded;
			} else {
				cursor_x += raw;
			}
			prev_cp = cp;
		}
	}

	// Phase 3: rebuild atlas textures (now includes all subpixel variants).
	_rebuild_dirty_atlases(face);

	// Phase 4: collect unique texture pages from the variants we'll actually use.
	std::set<int> used_pages;
	for (auto& e : entries) {
		auto it = face.glyph_index_cache.find(e.composite_key);
		if (it != face.glyph_index_cache.end() && it->second.has_geometry)
			used_pages.insert(it->second.texture_page);
	}

	if (used_pages.empty()) {
		float width = 0;
		for (auto& e : entries)
			width += e.kern_adj + e.advance + text_shaping_context.letter_spacing;
		return Rml::Math::Max(static_cast<int>(width), 0);
	}

	// Phase 5: assign mesh_list entries.
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
	if (!mesh_list.empty()) {
		mesh_list[0].mesh.vertices.reserve(entries.size() * 4);
		mesh_list[0].mesh.indices.reserve(entries.size() * 6);
	}

	// Phase 6: generate geometry using subpixel-shifted glyph data.
	float cursor_x = 0;
	float adv_rem = 0;
	for (auto& e : entries) {
		// Look up the shifted variant; fall back to glyph_index_cache base entry.
		const GlyphData* gd = nullptr;
		auto it = face.glyph_index_cache.find(e.composite_key);
		if (it != face.glyph_index_cache.end()) gd = &it->second;

		if (gd && gd->has_geometry) {
			auto pit = page_to_mesh.find(gd->texture_page);
			if (pit != page_to_mesh.end()) {
				int mi = pit->second;
				float gx = position.x + cursor_x + e.kern_adj + gd->origin.x;
				float gy = position.y + gd->origin.y;
				if (_pixel_snap || integer_advance) {
					gx = Rml::Math::RoundDown(gx);
					gy = Rml::Math::RoundDown(gy);
				}
				Rml::MeshUtilities::GenerateQuad(
					mesh_list[mi].mesh, Rml::Vector2f(gx, gy), gd->dimensions,
					colour, gd->uv_min, gd->uv_max);
			}
		}

		float raw = e.kern_adj + e.advance + text_shaping_context.letter_spacing;
		if (integer_advance) {
			raw += adv_rem;
			float rounded = Rml::Math::Round(raw);
			adv_rem = raw - rounded;
			cursor_x += rounded;
		} else {
			cursor_x += raw;
		}
	}

	return Rml::Math::Max(static_cast<int>(cursor_x), 0);
}

// Lay out + emit geometry using Godot's TextServer shaped-text pipeline. Glyph
// indices, advances and per-glyph offsets come straight from the shaper, so
// kerning/ligatures match Godot exactly. Glyphs are rendered from the same
// oversampled atlas as the other modes (cached by glyph index).
int GodotFontInterface::_generate_shaped(Rml::RenderManager& render_manager, FontFace& face,
	Rml::StringView string, Rml::Vector2f position, Rml::ColourbPremultiplied colour,
	const Rml::TextShapingContext& text_shaping_context, Rml::TexturedMeshList& mesh_list) {

	godot::Ref<godot::TextServer> ts = get_text_server();
	const float os = _oversample_factor();
	const int rsize = _render_size(face.size);
	const auto& font = _loaded_fonts[face.loaded_font_index];
	const int subpx_mode = _effective_subpixel_mode(font, rsize);

	godot::RID shaped = _shape_string(face, string);
	if (!shaped.is_valid()) return 0;

	godot::Array glyphs = ts->shaped_text_get_glyphs(shaped);
	const int glyph_count = static_cast<int>(glyphs.size());

	// Phase 1: ensure base glyph indices are rasterized (triggers font_render_glyph
	// which pre-renders all subpixel variants).
	for (int i = 0; i < glyph_count; i++) {
		godot::Dictionary g = glyphs[i];
		_ensure_glyph_index(face, static_cast<int64_t>(g["index"]));
	}

	// Phase 2: pre-compute positions and ensure subpixel-shifted variants.
	struct ShapedEntry { uint32_t composite_key; float x_off; float y_off; float advance; };
	std::vector<ShapedEntry> entries;
	entries.reserve(glyph_count);
	{
		float pen_x = position.x;
		for (int i = 0; i < glyph_count; i++) {
			godot::Dictionary g = glyphs[i];
			float x_off = static_cast<float>(static_cast<double>(g["x_off"])) / os;
			float y_off = static_cast<float>(static_cast<double>(g["y_off"])) / os;
			float advance = static_cast<float>(static_cast<double>(g["advance"])) / os;
			int64_t base_idx = static_cast<int64_t>(g["index"]);

			int shift = _compute_subpixel_shift(subpx_mode, pen_x + x_off);
			int64_t composite = base_idx | (static_cast<int64_t>(shift) << 27);
			_ensure_glyph_index(face, composite);

			entries.push_back({static_cast<uint32_t>(composite), x_off, y_off, advance});
			pen_x += advance + text_shaping_context.letter_spacing;
		}
	}

	// Phase 3: rebuild atlas (includes all subpixel variants).
	_rebuild_dirty_atlases(face);

	// Phase 4: collect unique texture pages.
	std::set<int> used_pages;
	for (auto& e : entries) {
		auto it = face.glyph_index_cache.find(e.composite_key);
		if (it != face.glyph_index_cache.end() && it->second.has_geometry)
			used_pages.insert(it->second.texture_page);
	}

	if (used_pages.empty()) {
		float total = 0;
		for (auto& e : entries) total += e.advance + text_shaping_context.letter_spacing;
		ts->free_rid(shaped);
		return Rml::Math::Max(static_cast<int>(total), 0);
	}

	// Phase 5: assign mesh_list entries per page.
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
	mesh_list[0].mesh.vertices.reserve(glyph_count * 4);
	mesh_list[0].mesh.indices.reserve(glyph_count * 6);

	// Phase 6: emit quads using subpixel-shifted glyph data.
	float pen_x = position.x;
	for (auto& e : entries) {
		auto it = face.glyph_index_cache.find(e.composite_key);
		if (it != face.glyph_index_cache.end() && it->second.has_geometry) {
			const GlyphData& gd = it->second;
			int mi = page_to_mesh[gd.texture_page];
			float gx = pen_x + e.x_off + gd.origin.x;
			float gy = position.y + e.y_off + gd.origin.y;
			if (_pixel_snap) {
				gx = Rml::Math::RoundDown(gx);
				gy = Rml::Math::RoundDown(gy);
			}
			Rml::MeshUtilities::GenerateQuad(
				mesh_list[mi].mesh, Rml::Vector2f(gx, gy), gd.dimensions,
				colour, gd.uv_min, gd.uv_max);
		}

		pen_x += e.advance + text_shaping_context.letter_spacing;
	}

	ts->free_rid(shaped);
	return Rml::Math::Max(static_cast<int>(pen_x - position.x), 0);
}

int GodotFontInterface::GetVersion(Rml::FontFaceHandle handle) {
	if (handle == 0 || handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return 0;
	return _faces[handle - 1]->version;
}

// --- Private: glyph caching ---

// Rasterize one glyph (by glyph index) at the oversampled size and produce a
// GlyphData with geometry divided back down to logical units, so layout is
// unchanged but the quad is textured with a higher-res glyph (minified on draw
// → crisp + bright, matching Godot's Label).
GodotFontInterface::GlyphData GodotFontInterface::_build_glyph_data(FontFace& face, int64_t glyph_index) {
	GlyphData glyph;
	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null() || face.loaded_font_index < 0 ||
		face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return glyph;

	const auto& font = _loaded_fonts[face.loaded_font_index];
	const float os = _oversample_factor();
	const int rsize = _render_size(face.size);
	godot::Vector2i size_v(rsize, 0);

	glyph.advance = static_cast<float>(ts->font_get_glyph_advance(font.font_rid, rsize, glyph_index).x) / os;

	godot::Vector2 glyph_size = ts->font_get_glyph_size(font.font_rid, size_v, glyph_index);
	if (glyph_size.x < 1 || glyph_size.y < 1) {
		glyph.has_geometry = false;
		return glyph;
	}

	ts->font_render_glyph(font.font_rid, size_v, glyph_index);

	godot::Vector2 offset = ts->font_get_glyph_offset(font.font_rid, size_v, glyph_index);
	godot::Rect2 uv_rect = ts->font_get_glyph_uv_rect(font.font_rid, size_v, glyph_index);
	godot::Vector2 tex_size = ts->font_get_glyph_texture_size(font.font_rid, size_v, glyph_index);
	int64_t tex_idx = ts->font_get_glyph_texture_idx(font.font_rid, size_v, glyph_index);

	glyph.texture_page = static_cast<int>(tex_idx);
	glyph.origin = Rml::Vector2f(static_cast<float>(offset.x) / os, static_cast<float>(offset.y) / os);
	glyph.dimensions = Rml::Vector2f(static_cast<float>(glyph_size.x) / os, static_cast<float>(glyph_size.y) / os);
	glyph.has_geometry = true;

	if (tex_size.x > 0 && tex_size.y > 0) {
		glyph.uv_min.x = static_cast<float>(uv_rect.position.x / tex_size.x);
		glyph.uv_min.y = static_cast<float>(uv_rect.position.y / tex_size.y);
		glyph.uv_max.x = static_cast<float>((uv_rect.position.x + uv_rect.size.x) / tex_size.x);
		glyph.uv_max.y = static_cast<float>((uv_rect.position.y + uv_rect.size.y) / tex_size.y);
	}

	face.dirty_pages.insert(glyph.texture_page);
	return glyph;
}

const GodotFontInterface::GlyphData& GodotFontInterface::_ensure_glyph(FontFace& face, uint32_t codepoint) {
	auto it = face.glyph_cache.find(codepoint);
	if (it != face.glyph_cache.end())
		return it->second;

	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null() || face.loaded_font_index < 0 ||
		face.loaded_font_index >= static_cast<int>(_loaded_fonts.size())) {
		static const GlyphData s_empty{};
		return s_empty;
	}
	const auto& font = _loaded_fonts[face.loaded_font_index];
	int64_t glyph_index = ts->font_get_glyph_index(font.font_rid, _render_size(face.size), codepoint, 0);
	face.codepoint_to_index[codepoint] = glyph_index;
	GlyphData glyph = _build_glyph_data(face, glyph_index);
	auto [inserted, _] = face.glyph_cache.emplace(codepoint, glyph);
	return inserted->second;
}

const GodotFontInterface::GlyphData& GodotFontInterface::_ensure_glyph_index(FontFace& face, int64_t glyph_index) {
	uint32_t key = static_cast<uint32_t>(glyph_index);
	auto it = face.glyph_index_cache.find(key);
	if (it != face.glyph_index_cache.end())
		return it->second;
	GlyphData glyph = _build_glyph_data(face, glyph_index);
	auto [inserted, _] = face.glyph_index_cache.emplace(key, glyph);
	return inserted->second;
}

godot::RID GodotFontInterface::_shape_string(const FontFace& face, Rml::StringView string) const {
	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null() || face.loaded_font_index < 0 ||
		face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return godot::RID();

	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::TypedArray<godot::RID> fonts;
	fonts.push_back(font.font_rid);
	if (_fallback_font_index >= 0 && _fallback_font_index < static_cast<int>(_loaded_fonts.size()) &&
		_fallback_font_index != face.loaded_font_index)
		fonts.push_back(_loaded_fonts[_fallback_font_index].font_rid);

	godot::RID shaped = ts->create_shaped_text();
	godot::String text = godot::String::utf8(Rml::String(string).c_str());
	ts->shaped_text_add_string(shaped, text, fonts, _render_size(face.size));
	ts->shaped_text_shape(shaped);
	return shaped;
}

// --- Private: atlas texture rebuild ---

void GodotFontInterface::_rebuild_dirty_atlases(FontFace& face) {
	if (face.dirty_pages.empty())
		return;

	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::RID font_rid = font.font_rid;
	// Must match the size used in _ensure_glyph so the atlas page indices and
	// glyph UVs line up with the oversampled rasterization.
	godot::Vector2i size_v(_render_size(face.size), 0);

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

			int w = atlas->get_width();
			int h = atlas->get_height();
			auto orig_format = atlas->get_format();

			if (orig_format == godot::Image::FORMAT_L8) {
				// Coverage stored as luminance — move to alpha, set RGB=white.
				godot::PackedByteArray src_data = atlas->get_data();
				godot::PackedByteArray data;
				data.resize(w * h * 4);
				uint8_t* dst = data.ptrw();
				const uint8_t* src = src_data.ptr();
				for (int i = 0; i < w * h; i++) {
					dst[i * 4 + 0] = 255;
					dst[i * 4 + 1] = 255;
					dst[i * 4 + 2] = 255;
					dst[i * 4 + 3] = src[i];
				}
				return tex_interface.GenerateTexture(
					Rml::Span<const Rml::byte>(data.ptr(), data.size()),
					Rml::Vector2i(w, h));
			}

			// LA8/RGBA8: coverage already in alpha — just ensure RGBA8.
			// Don't premultiply here; GenerateTexture handles it.
			godot::Ref<godot::Image> img = godot::Image::create_from_data(
				w, h, false, orig_format, atlas->get_data());
			if (img->get_format() != godot::Image::FORMAT_RGBA8)
				img->convert(godot::Image::FORMAT_RGBA8);

			godot::PackedByteArray data = img->get_data();
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
	if (ts.is_valid()) {
		for (auto& font : _loaded_fonts) {
			if (font.font_rid.is_valid() && !font.externally_owned)
				ts->free_rid(font.font_rid);
		}
	}
	_loaded_fonts.clear();
	_faces.clear();
	_fallback_font_index = -1;
}

void GodotFontInterface::_apply_font_settings(godot::RID font_rid) const {
	godot::Ref<godot::TextServer> ts = get_text_server();
	ts->font_set_hinting(font_rid, static_cast<godot::TextServer::Hinting>(_hinting));
	ts->font_set_antialiasing(font_rid, static_cast<godot::TextServer::FontAntialiasing>(_antialiasing));
	ts->font_set_subpixel_positioning(font_rid,
		static_cast<godot::TextServer::SubpixelPositioning>(_subpixel));
	ts->font_set_oversampling(font_rid, _oversampling);
	ts->font_set_keep_rounding_remainders(font_rid, true);
}

void GodotFontInterface::set_hinting(int hinting) {
	if (_hinting == hinting) return;
	_hinting = hinting;
	for (auto& font : _loaded_fonts) {
		if (!font.externally_owned) _apply_font_settings(font.font_rid);
	}
	_invalidate_all_caches();
}

void GodotFontInterface::set_font_antialiasing(int antialiasing) {
	if (_antialiasing == antialiasing) return;
	_antialiasing = antialiasing;
	for (auto& font : _loaded_fonts) {
		if (!font.externally_owned) _apply_font_settings(font.font_rid);
	}
	_invalidate_all_caches();
}

void GodotFontInterface::set_subpixel_positioning(int subpixel) {
	if (_subpixel == subpixel) return;
	_subpixel = subpixel;
	for (auto& font : _loaded_fonts) {
		if (!font.externally_owned) _apply_font_settings(font.font_rid);
	}
	_invalidate_all_caches();
}

void GodotFontInterface::set_font_oversampling(float oversampling) {
	if (_oversampling == oversampling) return;
	_oversampling = oversampling;
	for (auto& font : _loaded_fonts) {
		if (!font.externally_owned) _apply_font_settings(font.font_rid);
	}
	_invalidate_all_caches();
}

void GodotFontInterface::set_pixel_snap(bool snap) {
	if (_pixel_snap == snap) return;
	_pixel_snap = snap;
	// Only affects quad geometry in GenerateString; bump version so RmlUi
	// regenerates the cached strings.
	_invalidate_all_caches();
}

void GodotFontInterface::_invalidate_all_caches() {
	for (auto& face : _faces) {
		face->glyph_cache.clear();
		face->glyph_index_cache.clear();
		face->codepoint_to_index.clear();
		face->atlas_textures.clear();
		face->dirty_pages.clear();
		face->version++;
	}
}

int GodotFontInterface::_effective_subpixel_mode(const LoadedFont& font, int render_size) const {
	int mode;
	if (font.externally_owned) {
		godot::Ref<godot::TextServer> ts = get_text_server();
		mode = static_cast<int>(ts->font_get_subpixel_positioning(font.font_rid));
	} else {
		mode = _subpixel;
	}
	if (mode == godot::TextServer::SUBPIXEL_POSITIONING_AUTO) {
		if (render_size <= 16) return godot::TextServer::SUBPIXEL_POSITIONING_ONE_QUARTER;
		if (render_size <= 20) return godot::TextServer::SUBPIXEL_POSITIONING_ONE_HALF;
		return godot::TextServer::SUBPIXEL_POSITIONING_DISABLED;
	}
	return mode;
}

int GodotFontInterface::_compute_subpixel_shift(int subpixel_mode, float pen_x) {
	if (subpixel_mode == godot::TextServer::SUBPIXEL_POSITIONING_ONE_QUARTER) {
		return static_cast<int>(std::floor(4.0f * (pen_x + 0.125f))) -
		       4 * static_cast<int>(std::floor(pen_x + 0.125f));
	}
	if (subpixel_mode == godot::TextServer::SUBPIXEL_POSITIONING_ONE_HALF) {
		return static_cast<int>(std::floor(2.0f * (pen_x + 0.25f))) -
		       2 * static_cast<int>(std::floor(pen_x + 0.25f));
	}
	return 0;
}

void GodotFontInterface::set_layout_mode(int mode) {
	if (static_cast<int>(_layout_mode) == mode) return;
	_layout_mode = static_cast<LayoutMode>(mode);
	// Layout only affects geometry/metrics, not the atlas — but width changes,
	// so invalidate to force RmlUi to re-measure and regenerate strings.
	_invalidate_all_caches();

	const char* mode_names[] = {"Manual", "Integer Advance", "Shaped"};
	int idx = (mode >= 0 && mode <= 2) ? mode : 0;
	godot::UtilityFunctions::print(
		godot::String("[RmlUi Font] Layout mode: ") + godot::String(mode_names[idx]));
}

void GodotFontInterface::set_text_render_mode(TextRenderMode mode) {
	_text_render_mode = mode;

	// Map the preset onto the granular fields. Hinting/antialiasing stay at
	// their Label-matching defaults; the presets only toggle subpixel +
	// oversampling. Granular setters can still override afterwards.
	bool subpixel = (mode == TextRenderMode::SUBPIXEL || mode == TextRenderMode::HIGH_QUALITY);
	bool oversampled = (mode == TextRenderMode::OVERSAMPLED || mode == TextRenderMode::HIGH_QUALITY);
	_subpixel = subpixel ? godot::TextServer::SUBPIXEL_POSITIONING_ONE_QUARTER
	                     : godot::TextServer::SUBPIXEL_POSITIONING_DISABLED;
	_oversampling = oversampled ? 2.0f : 0.0f;

	for (auto& font : _loaded_fonts) {
		if (!font.externally_owned) _apply_font_settings(font.font_rid);
	}
	_invalidate_all_caches();

	const char* mode_names[] = {"Default", "Subpixel", "Oversampled", "High Quality"};
	godot::UtilityFunctions::print(
		godot::String("[RmlUi Font] Text render mode: ") +
		godot::String(mode_names[static_cast<int>(mode)]));
}

} // namespace RmlGodot
