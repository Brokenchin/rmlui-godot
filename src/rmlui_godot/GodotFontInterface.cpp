#include "GodotFontInterface.hpp"

#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/TextShapingContext.h>

#include <cmath>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
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

	if (weight == Rml::Style::FontWeight::Auto || weight == Rml::Style::FontWeight::Normal) {
		int ts_weight = static_cast<int>(ts->font_get_weight(font_rid));
		if (ts_weight > 0 && ts_weight <= 1000)
			weight = static_cast<Rml::Style::FontWeight>(ts_weight);
	}

	return _register_font(font_rid, family, style, weight, fallback_face);
}

bool GodotFontInterface::LoadFontFace(const Rml::String& file_name, int /*face_index*/,
	const Rml::String& family, Rml::Style::FontStyle style,
	Rml::Style::FontWeight weight, bool fallback_face) {

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

	if (weight == Rml::Style::FontWeight::Auto) {
		int ts_weight = static_cast<int>(ts->font_get_weight(font_rid));
		if (ts_weight > 0 && ts_weight <= 1000)
			weight = static_cast<Rml::Style::FontWeight>(ts_weight);
	}

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
	Rml::Style::FontWeight weight, const Rml::String& family_override) {

	if (!font_rid.is_valid()) return false;

	godot::Ref<godot::TextServer> ts = get_text_server();

	Rml::String family = family_override.empty()
		? Rml::String(ts->font_get_name(font_rid).utf8().get_data())
		: family_override;

	Rml::Style::FontStyle style = Rml::Style::FontStyle::Normal;
	if (static_cast<int64_t>(ts->font_get_style(font_rid)) & godot::TextServer::FONT_ITALIC)
		style = Rml::Style::FontStyle::Italic;

	if (weight == Rml::Style::FontWeight::Auto || weight == Rml::Style::FontWeight::Normal) {
		int ts_weight = static_cast<int>(ts->font_get_weight(font_rid));
		if (ts_weight > 0 && ts_weight <= 1000)
			weight = static_cast<Rml::Style::FontWeight>(ts_weight);
	}

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

		// Style must match exactly (CSS spec: style takes priority, no
		// cross-style fallback within a family).
		if (_loaded_fonts[i].style != style)
			continue;

		// Weight scored by closeness: max distance is 999 (1 to 1000), so
		// 1000 - distance gives higher scores for closer weights.
		int weight_dist = std::abs(
			static_cast<int>(_loaded_fonts[i].weight) - static_cast<int>(weight));
		int score = 1000 - weight_dist;

		if (score > best_score) {
			best_score = score;
			best_idx = i;
		}
	}
	return best_idx;
}

Rml::FontFaceHandle GodotFontInterface::GetFontFaceHandle(const Rml::String& family,
	Rml::Style::FontStyle style, Rml::Style::FontWeight weight, int size) {

	int font_idx = -1;

	// Parse comma-separated family list (e.g. "\"Noto Sans\", Arial, sans-serif").
	Rml::StringList families;
	Rml::StringUtilities::ExpandString(families, family, ',');
	for (auto& fam : families) {
		Rml::String trimmed = Rml::StringUtilities::StripWhitespace(fam);
		if (trimmed.size() >= 2 &&
			(trimmed.front() == '"' || trimmed.front() == '\'')) {
			trimmed = trimmed.substr(1, trimmed.size() - 2);
		}
		if (trimmed.empty())
			continue;

		font_idx = _find_font(trimmed, style, weight);
		if (font_idx >= 0)
			break;

		// Check generic family mapping (sans-serif, serif, monospace).
		std::string lower_key(to_lower(trimmed).c_str());
		auto git = _generic_families.find(lower_key);
		if (git != _generic_families.end()) {
			font_idx = _find_font(Rml::String(git->second.c_str()), style, weight);
			if (font_idx >= 0)
				break;
		}
	}

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
	float x_height = ascent * 0.5f;
	if (ts->font_has_char(font.font_rid, 'x')) {
		int64_t x_idx = ts->font_get_glyph_index(font.font_rid, size, 'x', 0);
		godot::Vector2 x_offset = ts->font_get_glyph_offset(font.font_rid, godot::Vector2i(size, 0), x_idx);
		godot::Vector2 x_size = ts->font_get_glyph_size(font.font_rid, godot::Vector2i(size, 0), x_idx);
		if (x_size.y > 0)
			x_height = static_cast<float>(x_size.y);
	}
	face->metrics.x_height = x_height;
	face->metrics.underline_position = static_cast<float>(ts->font_get_underline_position(font.font_rid, size));
	face->metrics.underline_thickness = static_cast<float>(ts->font_get_underline_thickness(font.font_rid, size));
	if (face->metrics.underline_thickness < 1.0f) face->metrics.underline_thickness = 1.0f;

	face->metrics.has_ellipsis = ts->font_has_char(font.font_rid, 0x2026);

	_faces.push_back(std::move(face));
	return static_cast<Rml::FontFaceHandle>(_faces.size());
}

Rml::FontEffectsHandle GodotFontInterface::PrepareFontEffects(Rml::FontFaceHandle handle,
	const Rml::FontEffectList& font_effects) {
	if (handle == 0 || handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return 0;
	if (font_effects.empty())
		return 0;

	FontFace& face = *_faces[handle - 1];

	if (face.layer_configs.empty())
		face.layer_configs.push_back({-1});

	for (size_t ci = 1; ci < face.layer_configs.size(); ci++) {
		const auto& config = face.layer_configs[ci];
		if (config.size() != font_effects.size() + 1)
			continue;
		size_t ei = 0;
		bool match = true;
		for (int idx : config) {
			if (idx == -1) continue;
			if (static_cast<size_t>(idx) >= face.effect_layers.size() ||
				face.effect_layers[idx]->effect.get() != font_effects[ei].get()) {
				match = false;
				break;
			}
			ei++;
		}
		if (match && ei == font_effects.size())
			return static_cast<Rml::FontEffectsHandle>(ci);
	}

	std::vector<int> config;
	bool added_base = false;

	for (size_t i = 0; i < font_effects.size(); i++) {
		if (!added_base && font_effects[i]->GetLayer() == Rml::FontEffect::Layer::Front) {
			config.push_back(-1);
			added_base = true;
		}

		int layer_idx = -1;
		for (size_t j = 0; j < face.effect_layers.size(); j++) {
			if (face.effect_layers[j]->effect.get() == font_effects[i].get()) {
				layer_idx = static_cast<int>(j);
				break;
			}
		}
		if (layer_idx < 0) {
			auto layer = std::make_unique<EffectLayer>();
			layer->effect = font_effects[i];
			layer->uses_base_textures = !font_effects[i]->HasUniqueTexture();
			layer_idx = static_cast<int>(face.effect_layers.size());
			face.effect_layers.push_back(std::move(layer));
		}
		config.push_back(layer_idx);
	}

	if (!added_base)
		config.push_back(-1);

	face.layer_configs.push_back(std::move(config));
	return static_cast<Rml::FontEffectsHandle>(face.layer_configs.size() - 1);
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
			const float rounded = Rml::Math::Round(raw);
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
	Rml::FontFaceHandle face_handle, Rml::FontEffectsHandle font_effects_handle,
	Rml::StringView string, Rml::Vector2f position, Rml::ColourbPremultiplied colour,
	float opacity, const Rml::TextShapingContext& text_shaping_context,
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
		return _generate_shaped(render_manager, face, string, position, colour, opacity,
			font_effects_handle, text_shaping_context, mesh_list);

	const bool integer_advance = (_layout_mode == LayoutMode::INTEGER_ADVANCE);
	bool use_kerning = (text_shaping_context.font_kerning != Rml::Style::FontKerning::None);
	const float os = _oversample_factor();
	const int rsize = _render_size(face.size);
	const int subpx_mode = _effective_subpixel_mode(font, rsize);

	// Phase 1: ensure base glyphs are cached.
	for (auto it = Rml::StringIteratorU8(string); it; ++it)
		_ensure_glyph(face, static_cast<uint32_t>(*it));

	// Phase 2: pre-compute positions and ensure subpixel-shifted variants.
	struct GlyphEntry { uint32_t composite_key; uint32_t codepoint; float kern_adj; float advance; float cursor_x; };
	std::vector<GlyphEntry> entries;
	float total_width = 0;
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

			entries.push_back({key, cp, kern_adj, base.advance, cursor_x});

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
		total_width = cursor_x;
	}

	// Phase 3: rebuild base atlas textures.
	_rebuild_dirty_atlases(face);

	// Resolve layer configuration.
	const std::vector<int>* config = nullptr;
	if (font_effects_handle > 0 &&
		static_cast<size_t>(font_effects_handle) < face.layer_configs.size())
		config = &face.layer_configs[static_cast<size_t>(font_effects_handle)];

	// Default: base-only
	std::vector<int> base_only = {-1};
	if (!config) config = &base_only;

	// Phase 4: ensure effect glyphs for unique-texture layers.
	for (int layer_idx : *config) {
		if (layer_idx < 0) continue;
		auto& layer = *face.effect_layers[layer_idx];
		if (layer.uses_base_textures) continue;
		for (auto& e : entries)
			_ensure_effect_glyph(face, layer, e.codepoint);
		_rebuild_effect_atlases(layer);
	}

	// --- Compositing: mode-specific position snap + UV padding ---
	const auto mode = (_text_render_mode == TextRenderMode::NONE)
		? TextRenderMode::RMLUI_NATIVE : _text_render_mode;
	const bool snap_x = _pixel_snap || integer_advance;

	// Position snap: isolates mode-specific glyph placement.
	// pen_x/y_off = pen offset from string origin (before glyph visual offset).
	// origin_x/y  = glyph visual offset from baseline.
	auto snap_pos = [&](float pen_x, float y_off, float origin_x, float origin_y) -> Rml::Vector2f {
		if (mode == TextRenderMode::SUBPIX_OFFSET) {
			float px = position.x + pen_x;
			float py = position.y + y_off;
			if (subpx_mode >= 3)     px += 0.125f; // ONE_QUARTER bias
			else if (subpx_mode == 2) px += 0.25f;  // ONE_HALF bias
			return {std::floor(px) + origin_x, std::floor(py) + origin_y};
		}
		// RMLUI_NATIVE
		float gx = position.x + pen_x + origin_x;
		float gy = Rml::Math::RoundDown(position.y + y_off + origin_y);
		if (subpx_mode == 0 && snap_x) gx = std::round(gx);
		return {gx, gy};
	};

	// UV padding: shrink by half-texel to prevent bilinear bleed between
	// adjacent glyphs in the atlas. Only active in SUBPIX_OFFSET mode.
	auto pad_uv = [&](Rml::Vector2f uv0, Rml::Vector2f uv1, float tw, float th) {
		if (mode == TextRenderMode::SUBPIX_OFFSET && tw > 0 && th > 0) {
			float hx = 0.5f / tw, hy = 0.5f / th;
			uv0.x += hx; uv0.y += hy;
			uv1.x -= hx; uv1.y -= hy;
		}
		return std::pair{uv0, uv1};
	};

	// Phase 5+6: generate mesh_list per layer.
	mesh_list.clear();
	int mesh_offset = 0;

	for (int layer_idx : *config) {
		bool is_base = (layer_idx == -1);
		EffectLayer* elayer = (!is_base && layer_idx >= 0 &&
			static_cast<size_t>(layer_idx) < face.effect_layers.size())
			? face.effect_layers[layer_idx].get() : nullptr;
		bool is_shadow = (elayer && elayer->uses_base_textures);
		bool is_unique = (elayer && !elayer->uses_base_textures);

		Rml::ColourbPremultiplied layer_colour = colour;
		if (elayer)
			layer_colour = elayer->effect->GetColour().ToPremultiplied(opacity);

		if (is_base || is_shadow) {
			std::set<int> used_pages;
			for (auto& e : entries) {
				auto it = face.glyph_index_cache.find(e.composite_key);
				if (it != face.glyph_index_cache.end() && it->second.has_geometry)
					used_pages.insert(it->second.texture_page);
			}
			if (used_pages.empty()) continue;

			int layer_start = mesh_offset;
			int layer_count = static_cast<int>(used_pages.size());
			mesh_list.resize(mesh_offset + layer_count);

			std::unordered_map<int, int> page_to_mesh;
			int mi = 0;
			for (int page : used_pages) {
				page_to_mesh[page] = layer_start + mi;
				auto tex_it = face.atlas_textures.find(page);
				if (tex_it != face.atlas_textures.end() && tex_it->second)
					mesh_list[layer_start + mi].texture = tex_it->second->GetTexture(render_manager);
				mi++;
			}
			mesh_list[layer_start].mesh.vertices.reserve(entries.size() * 4);
			mesh_list[layer_start].mesh.indices.reserve(entries.size() * 6);

			Rml::FontGlyph dummy_fg;
			dummy_fg.color_format = Rml::ColorFormat::A8;
			dummy_fg.bitmap_dimensions = Rml::Vector2i(1, 1);

			for (auto& e : entries) {
				const GlyphData* gd = nullptr;
				auto it = face.glyph_index_cache.find(e.composite_key);
				if (it != face.glyph_index_cache.end()) gd = &it->second;
				if (!gd || !gd->has_geometry) continue;

				auto pit = page_to_mesh.find(gd->texture_page);
				if (pit == page_to_mesh.end()) continue;

				auto pos = snap_pos(e.cursor_x + e.kern_adj, 0, gd->origin.x, gd->origin.y);

				if (is_shadow) {
					Rml::Vector2i eo(0, 0);
					Rml::Vector2i ed(static_cast<int>(gd->dimensions.x), static_cast<int>(gd->dimensions.y));
					if (!elayer->effect->GetGlyphMetrics(eo, ed, dummy_fg))
						continue;
					pos.x += static_cast<float>(eo.x);
					pos.y += static_cast<float>(eo.y);
				}

				auto [uv0, uv1] = pad_uv(gd->uv_min, gd->uv_max, gd->tex_w, gd->tex_h);
				Rml::MeshUtilities::GenerateQuad(
					mesh_list[pit->second].mesh, pos, gd->dimensions,
					layer_colour, uv0, uv1);
			}
			mesh_offset += layer_count;

		} else if (is_unique) {
			std::set<int> used_pages;
			for (auto& e : entries) {
				auto eg_it = elayer->glyph_cache.find(e.codepoint);
				if (eg_it != elayer->glyph_cache.end() && eg_it->second.has_geometry)
					used_pages.insert(eg_it->second.atlas_page);
			}
			if (used_pages.empty()) continue;

			int layer_start = mesh_offset;
			int layer_count = static_cast<int>(used_pages.size());
			mesh_list.resize(mesh_offset + layer_count);

			std::unordered_map<int, int> page_to_mesh;
			int mi = 0;
			for (int page : used_pages) {
				page_to_mesh[page] = layer_start + mi;
				auto tex_it = elayer->atlas_textures.find(page);
				if (tex_it != elayer->atlas_textures.end() && tex_it->second)
					mesh_list[layer_start + mi].texture = tex_it->second->GetTexture(render_manager);
				mi++;
			}
			mesh_list[layer_start].mesh.vertices.reserve(entries.size() * 4);
			mesh_list[layer_start].mesh.indices.reserve(entries.size() * 6);

			for (auto& e : entries) {
				auto eg_it = elayer->glyph_cache.find(e.codepoint);
				if (eg_it == elayer->glyph_cache.end() || !eg_it->second.has_geometry) continue;
				const auto& eg = eg_it->second;

				auto pit = page_to_mesh.find(eg.atlas_page);
				if (pit == page_to_mesh.end()) continue;

				auto pos = snap_pos(e.cursor_x + e.kern_adj, 0, eg.origin.x, eg.origin.y);
				constexpr float eff_atlas = static_cast<float>(EffectAtlasPage::SIZE);
				auto [uv0, uv1] = pad_uv(eg.uv_min, eg.uv_max, eff_atlas, eff_atlas);
				Rml::MeshUtilities::GenerateQuad(
					mesh_list[pit->second].mesh, pos, eg.dimensions,
					layer_colour, uv0, uv1);
			}
			mesh_offset += layer_count;
		}
	}

	return Rml::Math::Max(static_cast<int>(total_width), 0);
}

// Lay out + emit geometry using Godot's TextServer shaped-text pipeline. Glyph
// indices, advances and per-glyph offsets come straight from the shaper, so
// kerning/ligatures match Godot exactly. Glyphs are rendered from the same
// oversampled atlas as the other modes (cached by glyph index).
int GodotFontInterface::_generate_shaped(Rml::RenderManager& render_manager, FontFace& face,
	Rml::StringView string, Rml::Vector2f position, Rml::ColourbPremultiplied colour,
	float opacity, Rml::FontEffectsHandle font_effects_handle,
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

	// Phase 1: ensure base glyph indices are rasterized.
	for (int i = 0; i < glyph_count; i++) {
		godot::Dictionary g = glyphs[i];
		_ensure_glyph_index(face, static_cast<int64_t>(g["index"]));
	}

	// Phase 2: pre-compute positions and ensure subpixel-shifted variants.
	struct ShapedEntry { uint32_t composite_key; int64_t base_idx; float x_off; float y_off; float advance; float pen_x; };
	std::vector<ShapedEntry> entries;
	entries.reserve(glyph_count);
	float total_width = 0;
	{
		float pen_x = 0;
		for (int i = 0; i < glyph_count; i++) {
			godot::Dictionary g = glyphs[i];
			float x_off = static_cast<float>(static_cast<double>(g["x_off"])) / os;
			float y_off = static_cast<float>(static_cast<double>(g["y_off"])) / os;
			float advance = static_cast<float>(static_cast<double>(g["advance"])) / os;
			int64_t base_idx = static_cast<int64_t>(g["index"]);

			int shift = _compute_subpixel_shift(subpx_mode, position.x + pen_x + x_off);
			int64_t composite = base_idx | (static_cast<int64_t>(shift) << 27);
			_ensure_glyph_index(face, composite);

			entries.push_back({static_cast<uint32_t>(composite), base_idx, x_off, y_off, advance, pen_x});
			pen_x += advance + text_shaping_context.letter_spacing;
		}
		total_width = pen_x;
	}

	// Phase 3: rebuild atlas.
	_rebuild_dirty_atlases(face);

	// Resolve layer configuration.
	const std::vector<int>* config = nullptr;
	if (font_effects_handle > 0 &&
		static_cast<size_t>(font_effects_handle) < face.layer_configs.size())
		config = &face.layer_configs[static_cast<size_t>(font_effects_handle)];

	std::vector<int> base_only = {-1};
	if (!config) config = &base_only;

	// Phase 4: ensure effect glyphs for unique-texture layers.
	for (int layer_idx : *config) {
		if (layer_idx < 0) continue;
		auto& layer = *face.effect_layers[layer_idx];
		if (layer.uses_base_textures) continue;
		for (auto& e : entries)
			_ensure_effect_glyph_index(face, layer, e.base_idx);
		_rebuild_effect_atlases(layer);
	}

	// --- Compositing: mode-specific position snap + UV padding ---
	const auto mode = (_text_render_mode == TextRenderMode::NONE)
		? TextRenderMode::RMLUI_NATIVE : _text_render_mode;

	auto snap_pos = [&](float pen_x, float y_off, float origin_x, float origin_y) -> Rml::Vector2f {
		if (mode == TextRenderMode::SUBPIX_OFFSET) {
			float px = position.x + pen_x;
			float py = position.y + y_off;
			if (subpx_mode >= 3)     px += 0.125f;
			else if (subpx_mode == 2) px += 0.25f;
			return {std::floor(px) + origin_x, std::floor(py) + origin_y};
		}
		float gx = position.x + pen_x + origin_x;
		float gy = Rml::Math::RoundDown(position.y + y_off + origin_y);
		if (subpx_mode == 0 && _pixel_snap) gx = std::round(gx);
		return {gx, gy};
	};

	auto pad_uv = [&](Rml::Vector2f uv0, Rml::Vector2f uv1, float tw, float th) {
		if (mode == TextRenderMode::SUBPIX_OFFSET && tw > 0 && th > 0) {
			float hx = 0.5f / tw, hy = 0.5f / th;
			uv0.x += hx; uv0.y += hy;
			uv1.x -= hx; uv1.y -= hy;
		}
		return std::pair{uv0, uv1};
	};

	// Phase 5+6: generate mesh_list per layer.
	mesh_list.clear();
	int mesh_offset = 0;

	for (int layer_idx : *config) {
		bool is_base = (layer_idx == -1);
		EffectLayer* elayer = (!is_base && layer_idx >= 0 &&
			static_cast<size_t>(layer_idx) < face.effect_layers.size())
			? face.effect_layers[layer_idx].get() : nullptr;
		bool is_shadow = (elayer && elayer->uses_base_textures);
		bool is_unique = (elayer && !elayer->uses_base_textures);

		Rml::ColourbPremultiplied layer_colour = colour;
		if (elayer)
			layer_colour = elayer->effect->GetColour().ToPremultiplied(opacity);

		if (is_base || is_shadow) {
			std::set<int> used_pages;
			for (auto& e : entries) {
				auto it = face.glyph_index_cache.find(e.composite_key);
				if (it != face.glyph_index_cache.end() && it->second.has_geometry)
					used_pages.insert(it->second.texture_page);
			}
			if (used_pages.empty()) continue;

			int layer_start = mesh_offset;
			int layer_count = static_cast<int>(used_pages.size());
			mesh_list.resize(mesh_offset + layer_count);

			std::unordered_map<int, int> page_to_mesh;
			int mi = 0;
			for (int page : used_pages) {
				page_to_mesh[page] = layer_start + mi;
				auto tex_it = face.atlas_textures.find(page);
				if (tex_it != face.atlas_textures.end() && tex_it->second)
					mesh_list[layer_start + mi].texture = tex_it->second->GetTexture(render_manager);
				mi++;
			}
			mesh_list[layer_start].mesh.vertices.reserve(glyph_count * 4);
			mesh_list[layer_start].mesh.indices.reserve(glyph_count * 6);

			Rml::FontGlyph dummy_fg;
			dummy_fg.color_format = Rml::ColorFormat::A8;
			dummy_fg.bitmap_dimensions = Rml::Vector2i(1, 1);

			for (auto& e : entries) {
				auto it = face.glyph_index_cache.find(e.composite_key);
				if (it == face.glyph_index_cache.end() || !it->second.has_geometry) continue;
				const GlyphData& gd = it->second;

				auto pit = page_to_mesh.find(gd.texture_page);
				if (pit == page_to_mesh.end()) continue;

				auto pos = snap_pos(e.pen_x + e.x_off, e.y_off, gd.origin.x, gd.origin.y);

				if (is_shadow) {
					Rml::Vector2i eo(0, 0);
					Rml::Vector2i ed(static_cast<int>(gd.dimensions.x), static_cast<int>(gd.dimensions.y));
					if (!elayer->effect->GetGlyphMetrics(eo, ed, dummy_fg))
						continue;
					pos.x += static_cast<float>(eo.x);
					pos.y += static_cast<float>(eo.y);
				}

				auto [uv0, uv1] = pad_uv(gd.uv_min, gd.uv_max, gd.tex_w, gd.tex_h);
				Rml::MeshUtilities::GenerateQuad(
					mesh_list[pit->second].mesh, pos, gd.dimensions,
					layer_colour, uv0, uv1);
			}
			mesh_offset += layer_count;

		} else if (is_unique) {
			std::set<int> used_pages;
			for (auto& e : entries) {
				uint32_t key = static_cast<uint32_t>(e.base_idx);
				auto eg_it = elayer->glyph_index_cache.find(key);
				if (eg_it != elayer->glyph_index_cache.end() && eg_it->second.has_geometry)
					used_pages.insert(eg_it->second.atlas_page);
			}
			if (used_pages.empty()) continue;

			int layer_start = mesh_offset;
			int layer_count = static_cast<int>(used_pages.size());
			mesh_list.resize(mesh_offset + layer_count);

			std::unordered_map<int, int> page_to_mesh;
			int mi = 0;
			for (int page : used_pages) {
				page_to_mesh[page] = layer_start + mi;
				auto tex_it = elayer->atlas_textures.find(page);
				if (tex_it != elayer->atlas_textures.end() && tex_it->second)
					mesh_list[layer_start + mi].texture = tex_it->second->GetTexture(render_manager);
				mi++;
			}
			mesh_list[layer_start].mesh.vertices.reserve(glyph_count * 4);
			mesh_list[layer_start].mesh.indices.reserve(glyph_count * 6);

			for (auto& e : entries) {
				uint32_t key = static_cast<uint32_t>(e.base_idx);
				auto eg_it = elayer->glyph_index_cache.find(key);
				if (eg_it == elayer->glyph_index_cache.end() || !eg_it->second.has_geometry) continue;
				const auto& eg = eg_it->second;

				auto pit = page_to_mesh.find(eg.atlas_page);
				if (pit == page_to_mesh.end()) continue;

				auto pos = snap_pos(e.pen_x + e.x_off, e.y_off, eg.origin.x, eg.origin.y);
				constexpr float eff_atlas = static_cast<float>(EffectAtlasPage::SIZE);
				auto [uv0, uv1] = pad_uv(eg.uv_min, eg.uv_max, eff_atlas, eff_atlas);
				Rml::MeshUtilities::GenerateQuad(
					mesh_list[pit->second].mesh, pos, eg.dimensions,
					layer_colour, uv0, uv1);
			}
			mesh_offset += layer_count;
		}
	}

	ts->free_rid(shaped);
	return Rml::Math::Max(static_cast<int>(total_width), 0);
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
	glyph.tex_w = static_cast<float>(tex_size.x);
	glyph.tex_h = static_cast<float>(tex_size.y);
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

// --- Private: effect atlas ---

std::pair<int, int> GodotFontInterface::EffectAtlasPage::place(int w, int h, const uint8_t* rgba) {
	if (w <= 0 || h <= 0) return {-1, -1};
	if (cursor_x + w + 1 > SIZE) {
		cursor_y += row_height + 1;
		cursor_x = 1;
		row_height = 0;
	}
	if (cursor_y + h + 1 > SIZE)
		return {-1, -1};

	int x = cursor_x;
	int y = cursor_y;
	for (int row = 0; row < h; row++)
		memcpy(&pixels[((y + row) * SIZE + x) * 4], &rgba[row * w * 4], w * 4);
	cursor_x += w + 1;
	if (h > row_height) row_height = h;
	return {x, y};
}

std::vector<uint8_t> GodotFontInterface::_extract_glyph_alpha(const FontFace& face, int64_t glyph_index) const {
	if (face.loaded_font_index < 0 || face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return {};
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null()) return {};

	const int rsize = _render_size(face.size);
	godot::Vector2i size_v(rsize, 0);

	ts->font_render_glyph(font.font_rid, size_v, glyph_index);

	godot::Rect2 uv_rect = ts->font_get_glyph_uv_rect(font.font_rid, size_v, glyph_index);
	int64_t tex_idx = ts->font_get_glyph_texture_idx(font.font_rid, size_v, glyph_index);
	if (tex_idx < 0)
		return {};

	godot::Ref<godot::Image> atlas = ts->font_get_texture_image(font.font_rid, size_v, static_cast<int>(tex_idx));
	if (!atlas.is_valid() || atlas->is_empty())
		return {};

	int gw = static_cast<int>(uv_rect.size.x);
	int gh = static_cast<int>(uv_rect.size.y);
	if (gw <= 0 || gh <= 0)
		return {};

	int sx = static_cast<int>(uv_rect.position.x);
	int sy = static_cast<int>(uv_rect.position.y);
	int atlas_w = atlas->get_width();
	int atlas_h = atlas->get_height();
	auto format = atlas->get_format();

	godot::PackedByteArray src = atlas->get_data();
	const uint8_t* sp = src.ptr();

	std::vector<uint8_t> alpha(gw * gh, 0);
	for (int y = 0; y < gh; y++) {
		for (int x = 0; x < gw; x++) {
			int ax = sx + x;
			int ay = sy + y;
			if (ax < 0 || ax >= atlas_w || ay < 0 || ay >= atlas_h) continue;
			int pixel = ay * atlas_w + ax;
			uint8_t a = 0;
			if (format == godot::Image::FORMAT_L8)
				a = sp[pixel];
			else if (format == godot::Image::FORMAT_LA8)
				a = sp[pixel * 2 + 1];
			else if (format == godot::Image::FORMAT_RGBA8)
				a = sp[pixel * 4 + 3];
			alpha[y * gw + x] = a;
		}
	}
	return alpha;
}

GodotFontInterface::EffectGlyph GodotFontInterface::_build_effect_glyph(
	FontFace& face, EffectLayer& layer, int64_t glyph_index) {

	EffectGlyph eg;
	if (!layer.effect || layer.uses_base_textures) return eg;

	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();
	const int rsize = _render_size(face.size);
	godot::Vector2i size_v(rsize, 0);

	std::vector<uint8_t> alpha = _extract_glyph_alpha(face, glyph_index);
	if (alpha.empty()) return eg;

	godot::Rect2 uv_rect = ts->font_get_glyph_uv_rect(font.font_rid, size_v, glyph_index);
	int bw = static_cast<int>(uv_rect.size.x);
	int bh = static_cast<int>(uv_rect.size.y);
	if (bw <= 0 || bh <= 0) return eg;

	godot::Vector2 offset = ts->font_get_glyph_offset(font.font_rid, size_v, glyph_index);
	float os = _oversample_factor();

	Rml::FontGlyph fg;
	fg.bearing = Rml::Vector2i(static_cast<int>(offset.x), static_cast<int>(-offset.y));
	fg.advance = static_cast<int>(ts->font_get_glyph_advance(font.font_rid, rsize, glyph_index).x);
	fg.bitmap_data = alpha.data();
	fg.bitmap_dimensions = Rml::Vector2i(bw, bh);
	fg.color_format = Rml::ColorFormat::A8;

	Rml::Vector2i effect_origin(0, 0);
	Rml::Vector2i effect_dims = fg.bitmap_dimensions;
	if (!layer.effect->GetGlyphMetrics(effect_origin, effect_dims, fg))
		return eg;

	if (effect_dims.x <= 0 || effect_dims.y <= 0) return eg;

	std::vector<uint8_t> effect_rgba(effect_dims.x * effect_dims.y * 4, 0);
	int stride = effect_dims.x * 4;
	layer.effect->GenerateGlyphTexture(effect_rgba.data(), effect_dims, stride, fg);

	if (layer.atlas_pages.empty())
		layer.atlas_pages.emplace_back();

	int page_idx = static_cast<int>(layer.atlas_pages.size()) - 1;
	auto [px, py] = layer.atlas_pages[page_idx].place(effect_dims.x, effect_dims.y, effect_rgba.data());
	if (px < 0) {
		layer.atlas_pages.emplace_back();
		page_idx++;
		auto [px2, py2] = layer.atlas_pages[page_idx].place(effect_dims.x, effect_dims.y, effect_rgba.data());
		px = px2; py = py2;
	}
	if (px < 0) return eg;

	const auto& page = layer.atlas_pages[page_idx];
	eg.atlas_page = page_idx;
	eg.has_geometry = true;
	eg.uv_min.x = static_cast<float>(px) / static_cast<float>(EffectAtlasPage::SIZE);
	eg.uv_min.y = static_cast<float>(py) / static_cast<float>(EffectAtlasPage::SIZE);
	eg.uv_max.x = static_cast<float>(px + effect_dims.x) / static_cast<float>(EffectAtlasPage::SIZE);
	eg.uv_max.y = static_cast<float>(py + effect_dims.y) / static_cast<float>(EffectAtlasPage::SIZE);

	eg.origin.x = static_cast<float>(effect_origin.x + fg.bearing.x) / os;
	eg.origin.y = static_cast<float>(effect_origin.y - fg.bearing.y) / os;
	eg.dimensions.x = static_cast<float>(effect_dims.x) / os;
	eg.dimensions.y = static_cast<float>(effect_dims.y) / os;

	layer.dirty_pages.insert(page_idx);
	return eg;
}

void GodotFontInterface::_ensure_effect_glyph(FontFace& face, EffectLayer& layer, uint32_t codepoint) {
	if (layer.uses_base_textures) return;
	if (layer.glyph_cache.count(codepoint)) return;

	auto idx_it = face.codepoint_to_index.find(codepoint);
	if (idx_it == face.codepoint_to_index.end()) {
		layer.glyph_cache[codepoint] = EffectGlyph{};
		return;
	}
	layer.glyph_cache[codepoint] = _build_effect_glyph(face, layer, idx_it->second);
}

void GodotFontInterface::_ensure_effect_glyph_index(FontFace& face, EffectLayer& layer, int64_t glyph_index) {
	if (layer.uses_base_textures) return;
	uint32_t key = static_cast<uint32_t>(glyph_index);
	if (layer.glyph_index_cache.count(key)) return;
	int64_t base_idx = glyph_index & 0x07FFFFFF;
	layer.glyph_index_cache[key] = _build_effect_glyph(face, layer, base_idx);
}

void GodotFontInterface::_rebuild_effect_atlases(EffectLayer& layer) {
	if (layer.dirty_pages.empty()) return;

	for (int page_idx : layer.dirty_pages) {
		if (page_idx < 0 || page_idx >= static_cast<int>(layer.atlas_pages.size()))
			continue;
		auto pixel_data = std::make_shared<std::vector<uint8_t>>(
			layer.atlas_pages[page_idx].pixels);

		Rml::CallbackTextureFunction callback = [pixel_data](
			const Rml::CallbackTextureInterface& tex_interface) -> bool {
			return tex_interface.GenerateTexture(
				Rml::Span<const Rml::byte>(pixel_data->data(), pixel_data->size()),
				Rml::Vector2i(EffectAtlasPage::SIZE, EffectAtlasPage::SIZE));
		};
		layer.atlas_textures[page_idx] = std::make_unique<Rml::CallbackTextureSource>(std::move(callback));
	}
	layer.dirty_pages.clear();
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
		for (auto& layer : face->effect_layers) {
			for (auto& [page, tex_source] : layer->atlas_textures) {
				if (tex_source)
					tex_source->ReleaseForRenderManager(rm);
			}
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
	_invalidate_all_caches();
}

void GodotFontInterface::set_text_render_mode(int mode) {
	auto m = static_cast<TextRenderMode>(mode);
	if (_text_render_mode == m) return;
	_text_render_mode = m;
	_invalidate_all_caches();
}

void GodotFontInterface::_invalidate_all_caches() {
	for (auto& face : _faces) {
		face->glyph_cache.clear();
		face->glyph_index_cache.clear();
		face->codepoint_to_index.clear();
		face->atlas_textures.clear();
		face->dirty_pages.clear();
		for (auto& layer : face->effect_layers) {
			layer->glyph_cache.clear();
			layer->glyph_index_cache.clear();
			layer->atlas_pages.clear();
			layer->atlas_textures.clear();
			layer->dirty_pages.clear();
		}
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

void GodotFontInterface::set_generic_family(const Rml::String& generic, const Rml::String& mapped) {
	std::string key(to_lower(generic).c_str());
	if (mapped.empty()) {
		_generic_families.erase(key);
	} else {
		_generic_families[key] = std::string(mapped.c_str());
	}
}

Rml::String GodotFontInterface::get_generic_family(const Rml::String& generic) const {
	std::string key(to_lower(generic).c_str());
	auto it = _generic_families.find(key);
	if (it != _generic_families.end())
		return Rml::String(it->second.c_str());
	return {};
}

void GodotFontInterface::direct_draw_string(godot::RID canvas_item, Rml::FontFaceHandle handle,
	Rml::StringView string, godot::Vector2 position, godot::Color color) {

	if (handle == 0 || handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return;
	FontFace& face = *_faces[handle - 1];
	if (face.loaded_font_index < 0 || face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return;
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null()) return;

	godot::TypedArray<godot::RID> fonts;
	fonts.push_back(font.font_rid);
	if (_fallback_font_index >= 0 && _fallback_font_index < static_cast<int>(_loaded_fonts.size()) &&
		_fallback_font_index != face.loaded_font_index)
		fonts.push_back(_loaded_fonts[_fallback_font_index].font_rid);

	godot::RID shaped = ts->create_shaped_text();
	godot::String text = godot::String::utf8(Rml::String(string).c_str());
	ts->shaped_text_add_string(shaped, text, fonts, face.size);
	ts->shaped_text_shape(shaped);

	godot::Array glyphs = ts->shaped_text_get_glyphs(shaped);
	int count = static_cast<int>(glyphs.size());

	float pen_x = 0;
	for (int i = 0; i < count; i++) {
		godot::Dictionary g = glyphs[i];
		float x_off = static_cast<float>(static_cast<double>(g["x_off"]));
		float y_off = static_cast<float>(static_cast<double>(g["y_off"]));
		float advance = static_cast<float>(static_cast<double>(g["advance"]));
		int64_t index = static_cast<int64_t>(g["index"]);
		godot::RID glyph_font = g["font_rid"];

		if (glyph_font.is_valid()) {
			godot::Vector2 glyph_pos = position + godot::Vector2(pen_x + x_off, y_off);
			ts->font_draw_glyph(glyph_font, canvas_item, face.size, glyph_pos, index, color);
		}
		pen_x += advance;
	}

	ts->free_rid(shaped);
}

void GodotFontInterface::direct_mesh_draw_string(godot::RID canvas_item, Rml::FontFaceHandle handle,
	Rml::StringView string, godot::Vector2 position, godot::Color color) {

	if (handle == 0 || handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return;
	FontFace& face = *_faces[handle - 1];
	if (face.loaded_font_index < 0 || face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return;
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();
	auto* rs = godot::RenderingServer::get_singleton();
	if (ts.is_null() || rs == nullptr) return;

	godot::TypedArray<godot::RID> fonts;
	fonts.push_back(font.font_rid);
	if (_fallback_font_index >= 0 && _fallback_font_index < static_cast<int>(_loaded_fonts.size()) &&
		_fallback_font_index != face.loaded_font_index)
		fonts.push_back(_loaded_fonts[_fallback_font_index].font_rid);

	godot::RID shaped = ts->create_shaped_text();
	godot::String text = godot::String::utf8(Rml::String(string).c_str());
	ts->shaped_text_add_string(shaped, text, fonts, face.size);
	ts->shaped_text_shape(shaped);

	godot::Array glyphs = ts->shaped_text_get_glyphs(shaped);
	int count = static_cast<int>(glyphs.size());
	godot::Vector2i size_v(face.size, 0);

	float pen_x = 0;
	for (int i = 0; i < count; i++) {
		godot::Dictionary g = glyphs[i];
		float x_off = static_cast<float>(static_cast<double>(g["x_off"]));
		float y_off = static_cast<float>(static_cast<double>(g["y_off"]));
		float advance = static_cast<float>(static_cast<double>(g["advance"]));
		int64_t index = static_cast<int64_t>(g["index"]);
		godot::RID glyph_font = g["font_rid"];

		if (!glyph_font.is_valid()) { pen_x += advance; continue; }

		ts->font_render_glyph(glyph_font, size_v, index);

		godot::Vector2 glyph_size = ts->font_get_glyph_size(glyph_font, size_v, index);
		if (glyph_size.x < 1 || glyph_size.y < 1) { pen_x += advance; continue; }

		godot::Vector2 glyph_offset = ts->font_get_glyph_offset(glyph_font, size_v, index);
		godot::Rect2 uv_rect = ts->font_get_glyph_uv_rect(glyph_font, size_v, index);
		godot::RID tex_rid = ts->font_get_glyph_texture_rid(glyph_font, size_v, index);
		if (!tex_rid.is_valid()) { pen_x += advance; continue; }

		// UV padding: shrink by half-texel to prevent bilinear bleed
		godot::Vector2 tex_size = ts->font_get_glyph_texture_size(glyph_font, size_v, index);
		if (tex_size.x > 0 && tex_size.y > 0) {
			float hx = 0.5f / static_cast<float>(tex_size.x);
			float hy = 0.5f / static_cast<float>(tex_size.y);
			uv_rect.position.x += hx;
			uv_rect.position.y += hy;
			uv_rect.size.x -= 2.0f * hx;
			uv_rect.size.y -= 2.0f * hy;
		}

		// Replicate font_draw_glyph's internal position handling:
		godot::Vector2 pen_pos = position + godot::Vector2(pen_x + x_off, y_off);

		int subpx = static_cast<int>(ts->font_get_subpixel_positioning(glyph_font));
		if (subpx == 1) // AUTO
			subpx = (face.size <= 16) ? 3 : (face.size <= 20) ? 2 : 0;
		if (subpx == 3)      pen_pos.x += 0.125f;
		else if (subpx == 2) pen_pos.x += 0.25f;

		pen_pos.x = std::floor(pen_pos.x);
		pen_pos.y = std::floor(pen_pos.y);

		godot::Vector2 glyph_pos = pen_pos + glyph_offset;

		godot::Rect2 dst_rect(glyph_pos, glyph_size);
		rs->canvas_item_add_texture_rect_region(canvas_item, dst_rect, tex_rid, uv_rect,
			color, false, false);

		pen_x += advance;
	}
	ts->free_rid(shaped);
}

void GodotFontInterface::debug_dump_glyph_positions(Rml::FontFaceHandle handle, const Rml::String& text) {
	if (handle == 0 || handle > static_cast<Rml::FontFaceHandle>(_faces.size()))
		return;
	FontFace& face = *_faces[handle - 1];
	if (face.loaded_font_index < 0 || face.loaded_font_index >= static_cast<int>(_loaded_fonts.size()))
		return;
	const auto& font = _loaded_fonts[face.loaded_font_index];
	godot::Ref<godot::TextServer> ts = get_text_server();
	if (ts.is_null()) return;

	const float os = _oversample_factor();
	const int rsize = _render_size(face.size);
	const int subpx_mode = _effective_subpixel_mode(font, rsize);

	godot::UtilityFunctions::print(godot::String("\n=== GLYPH DEBUG DUMP === size=") +
		godot::String::num_int64(face.size) +
		godot::String(" rsize=") + godot::String::num_int64(rsize) +
		godot::String(" subpx=") + godot::String::num_int64(subpx_mode) +
		godot::String(" pixel_snap=") + godot::String(_pixel_snap ? "true" : "false") +
		godot::String(" layout=") + godot::String::num_int64(static_cast<int>(_layout_mode)));

	auto chr = [](uint32_t cp) -> godot::String {
		char32_t c = (cp >= 32 && cp < 127) ? static_cast<char32_t>(cp) : U'?';
		return godot::String::chr(c);
	};
	auto row = [](std::initializer_list<godot::String> cols) -> godot::String {
		godot::String r;
		for (auto& c : cols) { r += c + godot::String(" | "); }
		return r;
	};

	// --- Per-glyph layout with subpixel detail ---
	godot::UtilityFunctions::print(godot::String("--- MANUAL layout with subpixel ---"));
	godot::UtilityFunctions::print(godot::String("idx|ch|cursor_x |advance |shift|origin.x|origin_s|raw_gx  |snap_gx|godot_gx|width|gap"));

	float cursor_x = 0;
	int idx = 0;
	float prev_snap_gx = 0;
	float prev_width = 0;
	for (auto it = Rml::StringIteratorU8(text); it; ++it) {
		uint32_t cp = static_cast<uint32_t>(*it);
		const GlyphData& gd_base = _ensure_glyph(face, cp);

		int shift = _compute_subpixel_shift(subpx_mode, cursor_x);
		int64_t raw_idx = face.codepoint_to_index[cp];
		int64_t composite = raw_idx | (static_cast<int64_t>(shift) << 27);
		_ensure_glyph_index(face, composite);
		const GlyphData& gd_shift = face.glyph_index_cache[static_cast<uint32_t>(composite)];

		float raw_gx = cursor_x + gd_shift.origin.x;
		float snap_gx = Rml::Math::RoundDown(raw_gx);

		// Godot approach: floor(pen_x) + origin
		float godot_gx = std::floor(cursor_x) + gd_shift.origin.x;

		// Gap detection: does this glyph start after the previous one ends?
		float gap = 0;
		if (idx > 0 && gd_shift.has_geometry && prev_width > 0) {
			float prev_end = prev_snap_gx + prev_width;
			gap = snap_gx - prev_end;
		}

		godot::String gap_str = (gap > 0.5f) ? godot::String(" <<GAP>>") : godot::String("");

		godot::UtilityFunctions::print(row({
			godot::String::num_int64(idx), chr(cp),
			godot::String::num(cursor_x, 3),
			godot::String::num(gd_base.advance, 3),
			godot::String::num_int64(shift),
			godot::String::num(gd_base.origin.x, 2),
			godot::String::num(gd_shift.origin.x, 2),
			godot::String::num(raw_gx, 3),
			godot::String::num(snap_gx, 0),
			godot::String::num(godot_gx, 2),
			godot::String::num(gd_shift.dimensions.x, 0)
		}) + gap_str);

		if (gd_shift.has_geometry) {
			prev_snap_gx = snap_gx;
			prev_width = gd_shift.dimensions.x;
		}
		cursor_x += gd_base.advance;
		idx++;
	}

	// --- Godot shaped text layout ---
	godot::UtilityFunctions::print(godot::String("\n--- SHAPED layout (Godot) ---"));
	godot::UtilityFunctions::print(godot::String("idx|ch|pen_x    |x_off   |advance |adv_diff|pen_diff"));

	godot::RID shaped = _shape_string(face, text);
	if (shaped.is_valid()) {
		godot::Array glyphs = ts->shaped_text_get_glyphs(shaped);
		float shaped_pen = 0;
		float manual_pen = 0;
		for (int gi = 0; gi < static_cast<int>(glyphs.size()); gi++) {
			godot::Dictionary g = glyphs[gi];
			float adv = static_cast<float>(static_cast<double>(g["advance"])) / os;
			float x_off = static_cast<float>(static_cast<double>(g["x_off"])) / os;
			int64_t glyph_idx = static_cast<int64_t>(g["index"]);

			uint32_t cp = 0;
			for (auto& [k, v] : face.codepoint_to_index) {
				if (v == glyph_idx) { cp = k; break; }
			}
			const GlyphData& gd = _ensure_glyph(face, cp);

			float adv_diff = adv - gd.advance;
			float pen_diff = shaped_pen - manual_pen;

			godot::UtilityFunctions::print(row({
				godot::String::num_int64(gi), chr(cp),
				godot::String::num(shaped_pen, 3),
				godot::String::num(x_off, 3),
				godot::String::num(adv, 3),
				godot::String::num(adv_diff, 3),
				godot::String::num(pen_diff, 3)
			}));

			shaped_pen += adv;
			manual_pen += gd.advance;
		}
		godot::String total = godot::String("Shaped=") + godot::String::num(shaped_pen, 3);
		total += godot::String("  Manual=") + godot::String::num(manual_pen, 3);
		total += godot::String("  Diff=") + godot::String::num(shaped_pen - manual_pen, 3);
		godot::UtilityFunctions::print(total);
		ts->free_rid(shaped);
	}

	godot::UtilityFunctions::print(godot::String("=== END DUMP ===\n"));
}

} // namespace RmlGodot
