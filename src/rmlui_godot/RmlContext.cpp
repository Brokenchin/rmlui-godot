#include "RmlContext.hpp"
#include "RmlManager.hpp"
#include "RmlElementHandle.hpp"
#include "GodotEventListener.hpp"
#include "GodotFontInterface.hpp"

#include <algorithm>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StyleSheetContainer.h>
#include <RmlUi/Debugger.h>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector4.hpp>

namespace {

struct ClipVertex {
	godot::Vector2 pos;
	godot::Vector2 uv;
	godot::Color col;
};

static float _edge_dist(const godot::Vector2& p, int edge, const godot::Rect2& r) {
	switch (edge) {
		case 0: return p.x - r.position.x;                      // left
		case 1: return (r.position.x + r.size.x) - p.x;         // right
		case 2: return p.y - r.position.y;                       // top
		case 3: return (r.position.y + r.size.y) - p.y;          // bottom
		default: return 0.0f;
	}
}

static ClipVertex _lerp_vertex(const ClipVertex& a, const ClipVertex& b, float t) {
	return {
		a.pos.lerp(b.pos, t),
		a.uv.lerp(b.uv, t),
		a.col.lerp(b.col, t)
	};
}

static void _clip_polygon_edge(const std::vector<ClipVertex>& in, std::vector<ClipVertex>& out,
		int edge, const godot::Rect2& rect) {
	out.clear();
	if (in.empty()) return;
	for (size_t i = 0; i < in.size(); i++) {
		const ClipVertex& cur = in[i];
		const ClipVertex& prev = in[(i + in.size() - 1) % in.size()];
		float d_cur = _edge_dist(cur.pos, edge, rect);
		float d_prev = _edge_dist(prev.pos, edge, rect);
		if (d_prev >= 0.0f) {
			if (d_cur >= 0.0f) {
				out.push_back(cur);
			} else {
				float t = d_prev / (d_prev - d_cur);
				out.push_back(_lerp_vertex(prev, cur, t));
			}
		} else if (d_cur >= 0.0f) {
			float t = d_prev / (d_prev - d_cur);
			out.push_back(_lerp_vertex(prev, cur, t));
			out.push_back(cur);
		}
	}
}

struct ClipResult {
	godot::PackedVector2Array positions;
	godot::PackedColorArray colors;
	godot::PackedVector2Array uvs;
	godot::PackedInt32Array indices;
};

static bool _clip_mesh_to_rect(
		const RmlGodot::GodotRenderInterface::RawGeometry& raw,
		const godot::Transform2D& xform,
		const godot::Rect2& clip_rect,
		ClipResult& result) {

	const auto& src_pos = raw.positions;
	const auto& src_col = raw.colors;
	const auto& src_uv = raw.uvs;
	const auto& src_idx = raw.indices;

	result.positions.clear();
	result.colors.clear();
	result.uvs.clear();
	result.indices.clear();

	std::vector<ClipVertex> poly, buf;
	poly.reserve(8);
	buf.reserve(8);

	int vertex_base = 0;
	for (int64_t t = 0; t + 2 < src_idx.size(); t += 3) {
		poly.clear();
		for (int k = 0; k < 3; k++) {
			int vi = src_idx[t + k];
			ClipVertex v;
			v.pos = xform.xform(src_pos[vi]);
			v.uv = src_uv[vi];
			v.col = src_col[vi];
			poly.push_back(v);
		}

		for (int edge = 0; edge < 4; edge++) {
			_clip_polygon_edge(poly, buf, edge, clip_rect);
			std::swap(poly, buf);
		}

		if (poly.size() < 3) continue;

		for (auto& v : poly) {
			result.positions.push_back(v.pos);
			result.uvs.push_back(v.uv);
			result.colors.push_back(v.col);
		}
		for (size_t i = 1; i + 1 < poly.size(); i++) {
			result.indices.push_back(vertex_base);
			result.indices.push_back(vertex_base + static_cast<int>(i));
			result.indices.push_back(vertex_base + static_cast<int>(i) + 1);
		}
		vertex_base += static_cast<int>(poly.size());
	}

	return result.indices.size() > 0;
}

Rml::Variant godot_to_rml_variant(const godot::Variant& gv) {
	switch (gv.get_type()) {
		case godot::Variant::BOOL:
			return Rml::Variant(static_cast<bool>(gv));
		case godot::Variant::INT:
			return Rml::Variant(static_cast<int>(static_cast<int64_t>(gv)));
		case godot::Variant::FLOAT:
			return Rml::Variant(static_cast<float>(static_cast<double>(gv)));
		case godot::Variant::STRING: {
			godot::String s = gv;
			return Rml::Variant(Rml::String(s.utf8().get_data()));
		}
		case godot::Variant::VECTOR2: {
			godot::Vector2 v = gv;
			return Rml::Variant(Rml::Vector2f(v.x, v.y));
		}
		default:
			if (gv.get_type() != godot::Variant::NIL) {
				godot::String s = gv.stringify();
				return Rml::Variant(Rml::String(s.utf8().get_data()));
			}
			return Rml::Variant();
	}
}

godot::Variant rml_to_godot_variant(const Rml::Variant& rv) {
	switch (rv.GetType()) {
		case Rml::Variant::BOOL:
			return godot::Variant(rv.Get<bool>());
		case Rml::Variant::INT:
			return godot::Variant(static_cast<int64_t>(rv.Get<int>()));
		case Rml::Variant::INT64:
			return godot::Variant(rv.Get<int64_t>());
		case Rml::Variant::FLOAT:
			return godot::Variant(static_cast<double>(rv.Get<float>()));
		case Rml::Variant::DOUBLE:
			return godot::Variant(rv.Get<double>());
		case Rml::Variant::STRING:
			return godot::Variant(godot::String(rv.Get<Rml::String>().c_str()));
		case Rml::Variant::VECTOR2: {
			auto v = rv.Get<Rml::Vector2f>();
			return godot::Variant(godot::Vector2(v.x, v.y));
		}
		default:
			return godot::Variant();
	}
}

Rml::String godot_variant_to_rml_string(const godot::Variant& gv) {
	godot::String s = gv.stringify();
	return Rml::String(s.utf8().get_data());
}

Rml::Vector<Rml::String> godot_array_to_rml_string_vector(const godot::Array& arr) {
	Rml::Vector<Rml::String> result;
	result.reserve(arr.size());
	for (int i = 0; i < arr.size(); i++) {
		result.push_back(godot_variant_to_rml_string(arr[i]));
	}
	return result;
}

} // anonymous namespace

namespace RmlGodot {

RmlContext::RmlContext() {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager) {
		manager->on_context_created();
		_counted = true;
	}
}

RmlContext::~RmlContext() {
	_cleanup();

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		Rml::RenderManager* rm = Rml::GetRenderManager(&_render_interface);
		if (rm) {
			auto* font_iface = static_cast<RmlGodot::GodotFontInterface*>(
				Rml::GetFontEngineInterface());
			if (font_iface)
				font_iface->ReleaseTexturesForRenderManager(rm);
		}
		Rml::ReleaseTextures(&_render_interface);
		Rml::ReleaseCompiledGeometry(&_render_interface);
		Rml::ReleaseRenderManager(&_render_interface);
	}

	if (manager && _counted) {
		manager->on_context_destroyed();
	}
}

void RmlContext::_ready() {
	set_process(true);
	set_clip_contents(true);

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr) {
		godot::UtilityFunctions::push_error("[RmlUi] RmlManager singleton not available");
		return;
	}

	godot::Ref<godot::Material> editor_mat = get_material();
	if (editor_mat.is_valid()) {
		_active_material = editor_mat;
	} else {
		godot::Ref<godot::CanvasItemMaterial> premul;
		premul.instantiate();
		premul->set_blend_mode(godot::CanvasItemMaterial::BLEND_MODE_PREMULT_ALPHA);
		_active_material = premul;
		set_material(_active_material);
	}

	manager->ensure_initialized();
	_create_context();

	// Push granular font settings before loading faces so the first glyph
	// rasterization already uses them. These are authoritative; text_render_mode
	// remains a convenience preset that maps onto the same interface fields.
	auto& font_iface = manager->get_font_interface();
	font_iface.set_hinting(_font_hinting);
	font_iface.set_font_antialiasing(_font_antialiasing);
	font_iface.set_subpixel_positioning(_font_subpixel);
	font_iface.set_font_oversampling(_font_oversampling);
	font_iface.set_pixel_snap(_font_pixel_snap);
	font_iface.set_layout_mode(_font_layout_mode);

	for (int i = 0; i < _font_paths.size(); i++) {
		load_font_face(_font_paths[i]);
	}

	if (!_document_path.is_empty()) {
		load_document(_document_path);
	}
}

void RmlContext::_process(double /*delta*/) {
	if (_rml_context == nullptr) return;

	_sync_dimensions();
	_rml_context->Update();
	queue_redraw(); //what if nothing changed?
}

void RmlContext::_draw() {
	if (_rml_context == nullptr) return;

	_render_interface.clear_draw_commands();
	_rml_context->Render();

	const auto& commands = _render_interface.get_draw_commands();

	auto* rs = godot::RenderingServer::get_singleton();
	if (rs == nullptr) return;
	_free_scissor_items();
	_free_layer_items();

	if (!_active_material.is_valid()) return;
	godot::RID mat_rid = _active_material->get_rid();

	if (_gpu_scissor) _ensure_scissor_material();
	const bool use_gpu = _gpu_scissor && _scissor_material.is_valid();
	godot::RID scissor_mat_rid = use_gpu ? _scissor_material->get_rid() : godot::RID();

	ClipResult clip_buf;

	using CmdType = RmlGodot::GodotRenderInterface::CommandType;

	godot::Vector2 ctrl_size = get_size();

	godot::RID root_draw = rs->canvas_item_create();
	rs->canvas_item_set_parent(root_draw, get_canvas_item());
	rs->canvas_item_set_material(root_draw, mat_rid);
	_scissor_items.push_back(root_draw);

	struct LayerState {
		godot::RID canvas_item;
	};
	std::vector<LayerState> layer_stack;
	layer_stack.push_back({root_draw});

	godot::RID draw_target = root_draw;

	// Unified ordered sub-item pipeline.
	// Every drawable command paints into a child canvas item ("run") of the
	// current layer, NOT into the layer item directly. A new run is started
	// whenever the parent layer, the material, or (for GPU scissor) the scissor
	// rect changes, and each run is pinned with a strictly increasing draw index
	// so sibling paint order always matches command order. This is required
	// because a Godot canvas item draws its own commands BEFORE its children:
	// mixing direct draws with child items would reorder geometry. Routing
	// everything through ordered runs keeps z-index, GPU-scissor sub-items, and
	// decorator-shader material switches all consistent.
	godot::RID run_item;
	godot::RID run_parent;
	godot::RID run_material;
	bool run_scissored = false;
	godot::Rect2 run_rect;
	int run_draw_index = 0;

	auto invalidate_run = [&]() { run_item = godot::RID(); };

	auto target_for = [&](godot::RID parent, godot::RID material,
			bool scissored, const godot::Rect2& rect) -> godot::RID {
		if (run_item.is_valid() && run_parent == parent && run_material == material &&
			run_scissored == scissored && (!scissored || run_rect == rect)) {
			return run_item;
		}
		godot::RID item = rs->canvas_item_create();
		rs->canvas_item_set_parent(item, parent);
		rs->canvas_item_set_material(item, material);
		rs->canvas_item_set_draw_index(item, run_draw_index++);
		if (material == scissor_mat_rid) {
			godot::Vector4 rv = scissored
				? godot::Vector4(rect.position.x, rect.position.y, rect.size.x, rect.size.y)
				: godot::Vector4(-1000000.0f, -1000000.0f, 2000000.0f, 2000000.0f);
			rs->canvas_item_set_instance_shader_parameter(item, "scissor_rect", rv);
		}
		_scissor_items.push_back(item);
		run_item = item;
		run_parent = parent;
		run_material = material;
		run_scissored = scissored;
		run_rect = rect;
		return item;
	};

	for (int ci = 0; ci < static_cast<int>(commands.size()); ci++) {
		const auto& cmd = commands[ci];

		switch (cmd.type) {

		case CmdType::PUSH_LAYER: {
			invalidate_run();
			godot::RID group_item = rs->canvas_item_create();
			rs->canvas_item_set_parent(group_item, layer_stack.back().canvas_item);
			rs->canvas_item_set_material(group_item, mat_rid);
			rs->canvas_item_set_canvas_group_mode(group_item,
				godot::RenderingServer::CANVAS_GROUP_MODE_TRANSPARENT);
			_layer_items.push_back(group_item);
			layer_stack.push_back({group_item});
			draw_target = group_item;
			break;
		}

		case CmdType::COMPOSITE_LAYERS: {
			invalidate_run();
			if (layer_stack.size() < 2) break;
			godot::RID current_layer = layer_stack.back().canvas_item;

			float opacity = 1.0f;
			for (auto filter_handle : cmd.filters) {
				const auto* filter = _render_interface.get_filter(filter_handle);
				if (filter == nullptr) continue;
				if (filter->type == RmlGodot::GodotRenderInterface::FilterData::Type::OPACITY) {
					opacity *= filter->value;
				}
			}

			if (opacity < 1.0f) {
				rs->canvas_item_set_modulate(current_layer,
					godot::Color(1.0f, 1.0f, 1.0f, opacity));
			}
			break;
		}

		case CmdType::POP_LAYER: {
			invalidate_run();
			if (layer_stack.size() > 1) {
				layer_stack.pop_back();
				draw_target = layer_stack.back().canvas_item;
			}
			break;
		}

		case CmdType::ENABLE_CLIP_MASK: {
			invalidate_run();
			if (layer_stack.size() < 2) break;
			godot::RID current_layer = layer_stack.back().canvas_item;
			if (cmd.clip_mask_enabled) {
				rs->canvas_item_set_canvas_group_mode(current_layer,
					godot::RenderingServer::CANVAS_GROUP_MODE_CLIP_AND_DRAW);
			} else {
				rs->canvas_item_set_canvas_group_mode(current_layer,
					godot::RenderingServer::CANVAS_GROUP_MODE_TRANSPARENT);
			}
			break;
		}

		case CmdType::RENDER_TO_CLIP_MASK: {
			invalidate_run();
			godot::Ref<godot::ArrayMesh> mask_mesh = _render_interface.get_mesh(cmd.geometry);
			if (!mask_mesh.is_valid()) break;

			godot::Transform2D xform;
			if (cmd.has_transform) {
				xform = cmd.transform;
				xform.set_origin(xform.get_origin() + cmd.translation);
			} else {
				xform = godot::Transform2D();
				xform.set_origin(cmd.translation);
			}

			godot::Ref<godot::Texture2D> tex = _render_interface.get_texture_or_white(0);
			godot::RID tex_rid = tex.is_valid() ? tex->get_rid() : godot::RID();
			rs->canvas_item_add_mesh(draw_target, mask_mesh->get_rid(), xform,
				godot::Color(1, 1, 1, 1), tex_rid);
			break;
		}

		case CmdType::GEOMETRY:
		case CmdType::SHADER_GEOMETRY: {
			godot::Ref<godot::ArrayMesh> mesh = _render_interface.get_mesh(cmd.geometry);
			if (!mesh.is_valid()) continue;

			// Resolve the material for this draw. Ordinary geometry uses the
			// default canvas material; a decorator shader carries its own. If a
			// shader draw references an unregistered/invalid shader, fall back to
			// the default material so the geometry still renders.
			godot::RID geo_material = mat_rid;
			bool is_shader = false;
			if (cmd.type == CmdType::SHADER_GEOMETRY) {
				const auto* sd = _render_interface.get_shader(cmd.shader_handle);
				if (sd != nullptr && sd->material.is_valid()) {
					geo_material = sd->material->get_rid();
					is_shader = true;
				}
			}

			godot::Transform2D xform;
			if (cmd.has_transform) {
				xform = cmd.transform;
				xform.set_origin(xform.get_origin() + cmd.translation);
			} else {
				xform = godot::Transform2D();
				xform.set_origin(cmd.translation);
			}

			godot::AABB aabb3 = mesh->get_aabb();
			godot::Vector2 origin = xform.get_origin();
			float mesh_left   = origin.x + static_cast<float>(aabb3.position.x);
			float mesh_top    = origin.y + static_cast<float>(aabb3.position.y);
			float mesh_right  = mesh_left + static_cast<float>(aabb3.size.x);
			float mesh_bottom = mesh_top + static_cast<float>(aabb3.size.y);

			godot::Rect2 clip_rect(0, 0, ctrl_size.x, ctrl_size.y);
			if (cmd.scissor_enabled) {
				clip_rect = clip_rect.intersection(godot::Rect2(cmd.scissor_rect));
			}
			if (clip_rect.size.x <= 0 || clip_rect.size.y <= 0) continue;

			if (mesh_right  <= clip_rect.position.x ||
				mesh_left   >= clip_rect.position.x + clip_rect.size.x ||
				mesh_bottom <= clip_rect.position.y ||
				mesh_top    >= clip_rect.position.y + clip_rect.size.y) {
				continue;
			}

			draw_target = layer_stack.back().canvas_item;

			godot::Ref<godot::Texture2D> draw_tex = _render_interface.get_texture_or_white(cmd.texture);
			godot::RID tex_rid = draw_tex.is_valid() ? draw_tex->get_rid() : godot::RID();

			bool fully_inside = (mesh_left >= clip_rect.position.x &&
				mesh_top >= clip_rect.position.y &&
				mesh_right <= clip_rect.position.x + clip_rect.size.x &&
				mesh_bottom <= clip_rect.position.y + clip_rect.size.y);

			bool needs_scissor = cmd.scissor_enabled && !fully_inside;

			// GPU scissor only applies to ordinary geometry: a decorator shader
			// has its own material with no scissor uniform, so it always CPU-clips.
			bool gpu_path = use_gpu && !is_shader;

			if (gpu_path) {
				godot::RID target = target_for(draw_target, scissor_mat_rid, needs_scissor, clip_rect);
				rs->canvas_item_add_mesh(target, mesh->get_rid(), xform,
					godot::Color(1, 1, 1, 1), tex_rid);
			} else if (needs_scissor) {
				const auto* raw = _render_interface.get_raw_geometry(cmd.geometry);
				if (raw && _clip_mesh_to_rect(*raw, xform, clip_rect, clip_buf)) {
					godot::RID target = target_for(draw_target, geo_material, false, clip_rect);
					rs->canvas_item_add_triangle_array(target,
						clip_buf.indices, clip_buf.positions, clip_buf.colors,
						clip_buf.uvs, godot::PackedInt32Array(),
						godot::PackedFloat32Array(), tex_rid);
				}
			} else {
				godot::RID target = target_for(draw_target, geo_material, false, clip_rect);
				rs->canvas_item_add_mesh(target, mesh->get_rid(), xform,
					godot::Color(1, 1, 1, 1), tex_rid);
			}
			break;
		}

		} // switch
	}
}

void RmlContext::_notification(int p_what) {
	if (p_what == godot::Control::NOTIFICATION_RESIZED) {
		if (_rml_context != nullptr) {
			_sync_dimensions();
			_rml_context->Update();
			queue_redraw();
		}
	} else if (p_what == godot::Node::NOTIFICATION_EXIT_TREE) {
		_cleanup();
	}
}

void RmlContext::_gui_input(const godot::Ref<godot::InputEvent>& event) {
	if (_rml_context == nullptr) return;

	_forward_mouse_event(event);
	_forward_key_event(event);
}

void RmlContext::load_document(const godot::String& path) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load document — context not initialized");
		return;
	}

	if (path.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load document — path is empty");
		return;
	}

	Rml::String rml_path(path.utf8().get_data());

	Rml::ElementDocument* doc = _rml_context->LoadDocument(rml_path);
	if (doc != nullptr) {
		doc->Show();
		_sync_dimensions();
		_rml_context->Update();
		queue_redraw();

		_loaded_documents.push_back({std::string(rml_path.c_str()), doc});

		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Document loaded: ") + path);
	} else {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to load document: ") + path);
	}
}

bool RmlContext::reload_document(const godot::String& path) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot reload document — context not initialized");
		return false;
	}

	const Rml::String rml_path(path.utf8().get_data());
	std::string path_str(rml_path.c_str());

	auto it = std::find_if(_loaded_documents.begin(), _loaded_documents.end(),
		[&](const LoadedDocument& ld) { return ld.path == path_str; });

	if (it == _loaded_documents.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Document not loaded, cannot reload: ") + path);
		return false;
	}

	_listener_records.clear();

	if (it->document != nullptr) {
		_rml_context->UnloadDocument(it->document);
	}

	Rml::ElementDocument* new_doc = _rml_context->LoadDocument(rml_path);
	if (new_doc == nullptr) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to reload document: ") + path);
		_loaded_documents.erase(it);
		return false;
	}

	it->document = new_doc;
	new_doc->Show();

	for (auto& [name, entry] : _data_models) {
		entry.handle.DirtyAllVariables();
	}

	_sync_dimensions();
	_rml_context->Update();
	queue_redraw();

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Document reloaded: ") + path);
	return true;
}

void RmlContext::reload_all_documents() {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot reload — context not initialized");
		return;
	}

	_listener_records.clear();

	for (auto& ld : _loaded_documents) {
		if (ld.document != nullptr) {
			_rml_context->UnloadDocument(ld.document);
			ld.document = nullptr;
		}
	}

	for (auto& ld : _loaded_documents) {
		Rml::ElementDocument* doc = _rml_context->LoadDocument(Rml::String(ld.path));
		if (doc != nullptr) {
			doc->Show();
			ld.document = doc;
		} else {
			godot::UtilityFunctions::push_error(
				godot::String("[RmlUi] Failed to reload document: ") + godot::String(ld.path.c_str()));
		}
	}

	auto before_size = _loaded_documents.size();
	_loaded_documents.erase(
		std::remove_if(_loaded_documents.begin(), _loaded_documents.end(),
			[](const LoadedDocument& ld) { return ld.document == nullptr; }),
		_loaded_documents.end());

	if (_loaded_documents.size() < before_size) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Removed ") +
			godot::String::num_int64(static_cast<int64_t>(before_size - _loaded_documents.size())) +
			godot::String(" failed document(s) during reload"));
	}

	for (auto& [name, entry] : _data_models) {
		entry.handle.DirtyAllVariables();
	}

	_sync_dimensions();
	_rml_context->Update();
	queue_redraw();

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] All documents reloaded (") +
		godot::String::num_int64(static_cast<int64_t>(_loaded_documents.size())) +
		godot::String(" documents)"));
}

godot::Array RmlContext::get_loaded_documents() const {
	godot::Array result;
	for (const auto& ld : _loaded_documents) {
		result.append(godot::String(ld.path.c_str()));
	}
	return result;
}

bool RmlContext::load_font_face(const godot::String& path) {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load font — RmlUI not initialized");
		return false;
	}

	if (path.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load font — path is empty");
		return false;
	}

	Rml::String rml_path(path.utf8().get_data());
	bool ok = Rml::LoadFontFace(rml_path);
	if (ok) {
		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Font loaded: ") + path);
	} else {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to load font: ") + path);
	}
	return ok;
}

bool RmlContext::load_font_resource(const godot::Ref<godot::Font>& font) {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_error("[RmlUi] Cannot load font — RmlUI not initialized");
		return false;
	}
	if (!font.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot load font — null resource");
		return false;
	}

	auto& fi = manager->get_font_interface();
	godot::TypedArray<godot::RID> rids = font->get_rids();

	if (rids.is_empty()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Font resource has no TextServer RIDs");
		return false;
	}

	bool any_ok = false;
	for (int i = 0; i < rids.size(); i++) {
		godot::RID rid = rids[i];
		if (fi.LoadFontFromRID(rid, false, Rml::Style::FontWeight::Normal))
			any_ok = true;
	}

	if (any_ok) {
		godot::UtilityFunctions::print(
			godot::String("[RmlUi] Font resource loaded: ") + font->get_font_name());
	} else {
		godot::UtilityFunctions::push_error("[RmlUi] Failed to load font resource");
	}
	return any_ok;
}

// --- Private: Context lifecycle ---

void RmlContext::_create_context() {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) return;
	if (_rml_context != nullptr) return;

	godot::Vector2 size = get_size();
	if (size.x < 1 || size.y < 1) {
		size = godot::Vector2(800, 600);
	}

	Rml::String name(_context_name.utf8().get_data());
	_rml_context = Rml::CreateContext(name,
		Rml::Vector2i(static_cast<int>(size.x), static_cast<int>(size.y)),
		&_render_interface);

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_error("[RmlUi] Failed to create context");
		return;
	}

	_rml_context->SetDensityIndependentPixelRatio(_dp_ratio);

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Context created: ") + _context_name +
		godot::String(" (") + godot::String::num_int64(static_cast<int64_t>(size.x)) +
		godot::String("x") + godot::String::num_int64(static_cast<int64_t>(size.y)) +
		godot::String(")"));
}

void RmlContext::_destroy_context() {
	if (_rml_context == nullptr) return;

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		Rml::RemoveContext(_rml_context->GetName());
	}
	_rml_context = nullptr;
}

void RmlContext::_cleanup() {
	if (_rml_context == nullptr) return;

	auto* manager = RmlGodot::RmlManager::get_singleton();
	bool rmlui_alive = manager && manager->is_initialized();

	_listener_records.clear();

	if (rmlui_alive) {
		for (auto& ld : _loaded_documents) {
			if (ld.document != nullptr) {
				_rml_context->UnloadDocument(ld.document);
			}
		}
	}
	_loaded_documents.clear();

	if (rmlui_alive) {
		for (auto& [name, entry] : _data_models) {
			_rml_context->RemoveDataModel(Rml::String(name));
		}
	}
	_data_models.clear();

	_destroy_context();

	_free_scissor_items();
	_free_layer_items();

	_render_interface.release_all_resources();
}

// --- Public: dp_ratio ---

void RmlContext::set_dp_ratio(float ratio) {
	_dp_ratio = ratio;
	if (_rml_context != nullptr) {
		_rml_context->SetDensityIndependentPixelRatio(ratio);
	}
}

void RmlContext::set_text_render_mode(int mode) {
	if (_text_render_mode == mode) return;
	_text_render_mode = mode;

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_text_render_mode(
			static_cast<RmlGodot::GodotFontInterface::TextRenderMode>(mode));
		queue_redraw();
	}
}

void RmlContext::set_font_hinting(int hinting) {
	_font_hinting = hinting;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_hinting(hinting);
		queue_redraw();
	}
}

void RmlContext::set_font_antialiasing(int antialiasing) {
	_font_antialiasing = antialiasing;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_font_antialiasing(antialiasing);
		queue_redraw();
	}
}

void RmlContext::set_font_subpixel(int subpixel) {
	_font_subpixel = subpixel;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_subpixel_positioning(subpixel);
		queue_redraw();
	}
}

void RmlContext::set_font_oversampling(float oversampling) {
	_font_oversampling = oversampling;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_font_oversampling(oversampling);
		queue_redraw();
	}
}

void RmlContext::set_font_pixel_snap(bool snap) {
	_font_pixel_snap = snap;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_pixel_snap(snap);
		queue_redraw();
	}
}

void RmlContext::set_font_layout_mode(int mode) {
	_font_layout_mode = mode;
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		manager->get_font_interface().set_layout_mode(mode);
		reload_all_documents();
		queue_redraw();
	}
}

void RmlContext::set_gpu_scissor(bool enabled) {
	if (_gpu_scissor == enabled) return;
	_gpu_scissor = enabled;
	queue_redraw();
}

void RmlContext::_ensure_scissor_material() {
	if (_scissor_material.is_valid()) return;

	auto* loader = godot::ResourceLoader::get_singleton();
	godot::Ref<godot::Shader> shader = loader->load(
		"res://addons/rmlui-godot/shaders/rmlui_canvas_item.gdshader");
	if (!shader.is_valid()) {
		godot::UtilityFunctions::push_error(
			"[RmlUi] GPU scissor enabled but scissor shader could not be loaded; "
			"falling back to CPU clipping");
		return;
	}

	godot::Ref<godot::ShaderMaterial> mat;
	mat.instantiate();
	mat->set_shader(shader);
	_scissor_material = mat;
}

// --- Private: Dimension sync ---

void RmlContext::_sync_dimensions() {
	godot::Vector2 size = get_size();
	if (size.x < 1 || size.y < 1) return;

	Rml::Vector2i rml_size(static_cast<int>(size.x), static_cast<int>(size.y));
	if (_rml_context->GetDimensions() != rml_size) {
		_rml_context->SetDimensions(rml_size);
	}
}

void RmlContext::_free_scissor_items() {
	if (_scissor_items.empty()) return;
	auto* rs = godot::RenderingServer::get_singleton();
	for (auto& rid : _scissor_items) {
		rs->free_rid(rid);
	}
	_scissor_items.clear();
}

void RmlContext::_free_layer_items() {
	if (_layer_items.empty()) return;
	auto* rs = godot::RenderingServer::get_singleton();
	for (auto& rid : _layer_items) {
		rs->free_rid(rid);
	}
	_layer_items.clear();
}

// --- Public: Data binding ---

bool RmlContext::create_data_model(const godot::String& model_name) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot create data model — context not initialized");
		return false;
	}

	std::string name(model_name.utf8().get_data());
	if (_data_models.count(name)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model already exists: ") + model_name);
		return false;
	}

	Rml::DataModelConstructor constructor = _rml_context->CreateDataModel(Rml::String(name));
	if (!constructor) {
		godot::UtilityFunctions::push_error(
			godot::String("[RmlUi] Failed to create data model: ") + model_name);
		return false;
	}

	DataModelEntry entry;
	entry.constructor = constructor;
	entry.handle = constructor.GetModelHandle();
	_data_models[name] = std::move(entry);

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Data model created: ") + model_name);
	return true;
}

bool RmlContext::bind_data_variable(const godot::String& model_name,
	const godot::String& variable_name, const godot::Variant& initial_value) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot bind variable — context not initialized");
		return false;
	}

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return false;
	}

	std::string vname(variable_name.utf8().get_data());
	it->second.variables[vname] = godot_to_rml_variant(initial_value);

	auto* vars = &it->second.variables;
	std::string captured_vname = vname;

	it->second.constructor.BindFunc(
		Rml::String(vname),
		[vars, captured_vname](Rml::Variant& variant) {
			auto found = vars->find(captured_vname);
			if (found != vars->end())
				variant = found->second;
		},
		[vars, captured_vname](const Rml::Variant& variant) {
			(*vars)[captured_vname] = variant;
		}
	);

	return true;
}

void RmlContext::set_data_variable(const godot::String& model_name,
	const godot::String& variable_name, const godot::Variant& value) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot set data variable — context not initialized");
		return;
	}

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return;
	}

	std::string vname(variable_name.utf8().get_data());
	it->second.variables[vname] = godot_to_rml_variant(value);
	it->second.handle.DirtyVariable(Rml::String(vname));
}

godot::Variant RmlContext::get_data_variable(const godot::String& model_name,
	const godot::String& variable_name) const {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) return godot::Variant();

	std::string vname(variable_name.utf8().get_data());
	auto vit = it->second.variables.find(vname);
	if (vit == it->second.variables.end()) return godot::Variant();

	return rml_to_godot_variant(vit->second);
}

bool RmlContext::bind_data_event(const godot::String& model_name,
	const godot::String& event_name, const godot::Callable& callable) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot bind event — context not initialized");
		return false;
	}

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return false;
	}

	std::string ename(event_name.utf8().get_data());
	it->second.event_callbacks[ename] = callable;

	auto* callbacks = &it->second.event_callbacks;
	std::string captured_ename = ename;

	it->second.constructor.BindEventCallback(
		Rml::String(ename),
		[callbacks, captured_ename](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/,
			const Rml::VariantList& arguments) {
			auto found = callbacks->find(captured_ename);
			if (found == callbacks->end()) return;

			godot::Array args;
			for (const auto& arg : arguments) {
				args.append(rml_to_godot_variant(arg));
			}
			found->second.callv(args);
		}
	);

	return true;
}

void RmlContext::dirty_data_variable(const godot::String& model_name,
	const godot::String& variable_name) {

	if (_rml_context == nullptr) return;

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) return;

	it->second.handle.DirtyVariable(Rml::String(std::string(variable_name.utf8().get_data())));
}

void RmlContext::dirty_all_variables(const godot::String& model_name) {
	if (_rml_context == nullptr) return;

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) return;

	it->second.handle.DirtyAllVariables();
}

bool RmlContext::create_data_model_from_dict(const godot::String& model_name,
	const godot::Dictionary& variables) {

	if (!create_data_model(model_name)) return false;

	godot::Array keys = variables.keys();
	for (int i = 0; i < keys.size(); i++) {
		godot::String key = keys[i];
		bind_data_variable(model_name, key, variables[key]);
	}

	return true;
}

void RmlContext::update_data_model(const godot::String& model_name,
	const godot::Dictionary& variables) {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return;
	}

	godot::Array keys = variables.keys();
	for (int i = 0; i < keys.size(); i++) {
		godot::String key = keys[i];
		std::string vname(key.utf8().get_data());
		auto var_it = it->second.variables.find(vname);
		if (var_it != it->second.variables.end()) {
			var_it->second = godot_to_rml_variant(variables[key]);
			it->second.handle.DirtyVariable(Rml::String(vname));
		}
	}
}

// --- Phase 3: Array data binding ---

bool RmlContext::bind_data_array(const godot::String& model_name,
	const godot::String& array_name, const godot::Array& initial_array) {

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot bind array — context not initialized");
		return false;
	}

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return false;
	}

	std::string aname(array_name.utf8().get_data());
	if (it->second.arrays.count(aname)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array already bound: ") + array_name);
		return false;
	}

	if (!RmlGodot::RmlManager::get_singleton()->is_array_type_registered()) {
		it->second.constructor.RegisterArray<Rml::Vector<Rml::String>>();
		RmlGodot::RmlManager::get_singleton()->set_array_type_registered(true);
	}

	it->second.arrays[aname] = godot_array_to_rml_string_vector(initial_array);

	auto* array_ptr = &it->second.arrays[aname];
	it->second.constructor.Bind(Rml::String(aname), array_ptr);

	return true;
}

void RmlContext::set_data_array(const godot::String& model_name,
	const godot::String& array_name, const godot::Array& array) {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return;
	}

	std::string aname(array_name.utf8().get_data());
	auto ait = it->second.arrays.find(aname);
	if (ait == it->second.arrays.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array not bound: ") + array_name);
		return;
	}

	ait->second = godot_array_to_rml_string_vector(array);
	it->second.handle.DirtyVariable(Rml::String(aname));
}

void RmlContext::push_data_array_item(const godot::String& model_name,
	const godot::String& array_name, const godot::Variant& value) {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return;
	}

	std::string aname(array_name.utf8().get_data());
	auto ait = it->second.arrays.find(aname);
	if (ait == it->second.arrays.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array not bound: ") + array_name);
		return;
	}

	ait->second.push_back(godot_variant_to_rml_string(value));
	it->second.handle.DirtyVariable(Rml::String(aname));
}

void RmlContext::remove_data_array_item(const godot::String& model_name,
	const godot::String& array_name, int index) {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return;
	}

	std::string aname(array_name.utf8().get_data());
	auto ait = it->second.arrays.find(aname);
	if (ait == it->second.arrays.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array not bound: ") + array_name);
		return;
	}

	if (index < 0 || index >= static_cast<int>(ait->second.size())) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array index out of bounds: ") + godot::String::num_int64(index));
		return;
	}

	ait->second.erase(ait->second.begin() + index);
	it->second.handle.DirtyVariable(Rml::String(aname));
}

void RmlContext::set_data_array_item(const godot::String& model_name,
	const godot::String& array_name, int index, const godot::Variant& value) {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return;
	}

	std::string aname(array_name.utf8().get_data());
	auto ait = it->second.arrays.find(aname);
	if (ait == it->second.arrays.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array not bound: ") + array_name);
		return;
	}

	if (index < 0 || index >= static_cast<int>(ait->second.size())) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array index out of bounds: ") + godot::String::num_int64(index));
		return;
	}

	ait->second[index] = godot_variant_to_rml_string(value);
	it->second.handle.DirtyVariable(Rml::String(aname));
}

int RmlContext::get_data_array_size(const godot::String& model_name,
	const godot::String& array_name) const {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) return 0;

	std::string aname(array_name.utf8().get_data());
	auto ait = it->second.arrays.find(aname);
	if (ait == it->second.arrays.end()) return 0;

	return static_cast<int>(ait->second.size());
}

void RmlContext::clear_data_array(const godot::String& model_name,
	const godot::String& array_name) {

	std::string mname(model_name.utf8().get_data());
	auto it = _data_models.find(mname);
	if (it == _data_models.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Data model not found: ") + model_name);
		return;
	}

	std::string aname(array_name.utf8().get_data());
	auto ait = it->second.arrays.find(aname);
	if (ait == it->second.arrays.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Array not bound: ") + array_name);
		return;
	}

	ait->second.clear();
	it->second.handle.DirtyVariable(Rml::String(aname));
}

// --- Phase 5: Custom element instancers ---

bool RmlContext::register_custom_element(const godot::String& tag_name,
	const godot::Callable& on_create, const godot::Callable& on_attribute_change) {

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot register custom element — RmlUI not initialized");
		return false;
	}

	if (!on_create.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot register custom element — on_create callable is invalid");
		return false;
	}

	std::string tag(tag_name.utf8().get_data());

	bool already_registered = false;
	auto& tags = manager->get_registered_tags();
	for (const auto& t : tags) {
		if (t == tag) {
			already_registered = true;
			break;
		}
	}

	RmlGodot::GodotElementInstancer::TagCallbacks callbacks;
	callbacks.on_create = on_create;
	callbacks.on_attribute_change = on_attribute_change;
	manager->get_element_instancer().register_tag(tag, std::move(callbacks));

	if (!already_registered) {
		Rml::Factory::RegisterElementInstancer(Rml::String(tag), &manager->get_element_instancer());
		tags.push_back(tag);
	}

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Custom element registered: <") + tag_name + godot::String(">"));
	return true;
}

// --- Phase 1: DOM events & element access ---

Rml::Element* RmlContext::_find_element(const godot::String& id) const {
	if (_rml_context == nullptr) return nullptr;

	Rml::String rml_id(id.utf8().get_data());
	int num_docs = _rml_context->GetNumDocuments();
	for (int i = 0; i < num_docs; i++) {
		Rml::ElementDocument* doc = _rml_context->GetDocument(i);
		if (doc == nullptr) continue;
		Rml::Element* el = doc->GetElementById(rml_id);
		if (el != nullptr) return el;
	}
	return nullptr;
}

bool RmlContext::add_event_listener(const godot::String& element_id,
	const godot::String& event_type, const godot::Callable& callable, bool in_capture_phase) {

	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] add_event_listener — element not found: ") + element_id);
		return false;
	}

	std::string type_str(event_type.utf8().get_data());
	auto* listener = new RmlGodot::GodotEventListener(callable, type_str);
	el->AddEventListener(Rml::String(type_str), listener, in_capture_phase);

	ListenerRecord record;
	record.element = el;
	record.listener = listener;
	record.event_type = type_str;
	record.in_capture_phase = in_capture_phase;
	_listener_records.push_back(record);

	return true;
}

void RmlContext::remove_event_listeners(const godot::String& element_id,
	const godot::String& event_type) {

	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] remove_event_listeners — element not found: ") + element_id);
		return;
	}

	std::string type_str(event_type.utf8().get_data());

	auto it = _listener_records.begin();
	while (it != _listener_records.end()) {
		if (it->element == el && it->event_type == type_str) {
			el->RemoveEventListener(Rml::String(type_str), it->listener, it->in_capture_phase);
			it = _listener_records.erase(it);
		} else {
			++it;
		}
	}
}

godot::Ref<RmlElementHandle> RmlContext::get_element_by_id(const godot::String& id) const
{
	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();

	if (Rml::Element* el = _find_element(id); el != nullptr) {
		handle->set_element(el);
	} else {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_element_by_id — element not found: ") + id);
	}

	return handle;
}

bool RmlContext::set_element_property(const godot::String& element_id,
	const godot::String& property, const godot::String& value) {

	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_property — element not found: ") + element_id);
		return false;
	}

	return el->SetProperty(
		Rml::String(property.utf8().get_data()),
		Rml::String(value.utf8().get_data()));
}

void RmlContext::remove_element_property(const godot::String& element_id,
	const godot::String& property) {

	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] remove_element_property — element not found: ") + element_id);
		return;
	}

	el->RemoveProperty(Rml::String(property.utf8().get_data()));
}

void RmlContext::set_element_class(const godot::String& element_id,
	const godot::String& class_name, bool activate) {

	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_class — element not found: ") + element_id);
		return;
	}

	el->SetClass(Rml::String(class_name.utf8().get_data()), activate);
}

void RmlContext::set_element_inner_rml(const godot::String& element_id, const godot::String& rml) {
	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_inner_rml — element not found: ") + element_id);
		return;
	}

	el->SetInnerRML(Rml::String(rml.utf8().get_data()));
}

godot::String RmlContext::get_element_outer_rml(const godot::String& element_id) const {
	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] get_element_outer_rml — element not found: ") + element_id);
		return {};
	}

	godot::Ref<RmlElementHandle> handle;
	handle.instantiate();
	handle->set_element(el);
	return handle->get_outer_rml();
}

godot::String RmlContext::get_element_attribute(const godot::String& element_id,
	const godot::String& attribute, const godot::String& default_value) const {

	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) return default_value;

	const Rml::Variant* attr = el->GetAttribute(Rml::String(attribute.utf8().get_data()));
	if (attr == nullptr) return default_value;
	return godot::String(attr->Get<Rml::String>().c_str());
}

void RmlContext::set_element_attribute(const godot::String& element_id,
	const godot::String& attribute, const godot::String& value) {

	Rml::Element* el = _find_element(element_id);
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] set_element_attribute — element not found: ") + element_id);
		return;
	}

	el->SetAttribute(
		Rml::String(attribute.utf8().get_data()),
		Rml::String(value.utf8().get_data()));
}

// --- Texture registration ---

bool RmlContext::register_texture(const godot::String& name, const godot::Ref<godot::Texture2D>& texture) {
	if (!texture.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] register_texture — texture is null");
		return false;
	}
	std::string key(name.utf8().get_data());
	if (!_render_interface.register_texture(key, texture)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] register_texture failed: ") + name);
		return false;
	}
	return true;
}

bool RmlContext::unregister_texture(const godot::String& name) {
	std::string key(name.utf8().get_data());
	return _render_interface.unregister_texture(key);
}

// --- Decorator shader registration ---

bool RmlContext::register_decorator_shader(const godot::String& name, const godot::Ref<godot::Shader>& shader) {
	if (!shader.is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] register_decorator_shader — shader is null");
		return false;
	}
	std::string key(name.utf8().get_data());
	if (!_render_interface.register_shader(key, shader)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] register_decorator_shader failed: ") + name);
		return false;
	}
	return true;
}

bool RmlContext::register_decorator_material(const godot::String& name, const godot::Ref<godot::ShaderMaterial>& material) {
	if (!material.is_valid() || !material->get_shader().is_valid()) {
		godot::UtilityFunctions::push_warning("[RmlUi] register_decorator_material — material or its shader is null");
		return false;
	}
	std::string key(name.utf8().get_data());
	if (!_render_interface.register_shader_material(key, material)) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] register_decorator_material failed: ") + name);
		return false;
	}
	return true;
}

bool RmlContext::unregister_decorator_shader(const godot::String& name) {
	std::string key(name.utf8().get_data());
	return _render_interface.unregister_shader(key);
}

// --- A4: Drag-and-drop (gd_drag interop) ---

void RmlContext::register_drag_source(const godot::String& element_id,
	const godot::Callable& payload_builder, const godot::Callable& ghost_builder) {

	std::string id(element_id.utf8().get_data());
	for (const auto& src : _drag_sources) {
		if (src.element_id == id) {
			godot::UtilityFunctions::push_warning(
				godot::String("[RmlUi] Drag source already registered: ") + element_id);
			return;
		}
	}
	_drag_sources.push_back({id, payload_builder, ghost_builder});
}

void RmlContext::register_drop_target(const godot::String& element_id,
	const godot::Callable& drop_handler) {

	std::string id(element_id.utf8().get_data());
	for (const auto& tgt : _drop_targets) {
		if (tgt.element_id == id) {
			godot::UtilityFunctions::push_warning(
				godot::String("[RmlUi] Drop target already registered: ") + element_id);
			return;
		}
	}
	_drop_targets.push_back({id, drop_handler});
}

bool RmlContext::_point_in_element(Rml::Element* el, float x, float y) const {
	Rml::Vector2f offset = el->GetAbsoluteOffset(Rml::BoxArea::Border);
	Rml::Vector2f size = el->GetBox().GetSize(Rml::BoxArea::Border);
	return x >= offset.x && x <= offset.x + size.x
		&& y >= offset.y && y <= offset.y + size.y;
}

godot::Variant RmlContext::_get_drag_data(const godot::Vector2& p_at_position) {
	if (_rml_context == nullptr || _drag_sources.empty()) return {};

	for (const auto& source : _drag_sources) {
		Rml::Element* el = _find_element(godot::String(source.element_id.c_str()));
		if (el == nullptr) continue;

		if (!_point_in_element(el, p_at_position.x, p_at_position.y)) continue;

		godot::Dictionary payload;
		payload["_rml_source"] = true;
		payload["_element_id"] = godot::String(source.element_id.c_str());

		if (source.payload_builder.is_valid()) {
			godot::Variant result = source.payload_builder.call(
				godot::String(source.element_id.c_str()), p_at_position);
			if (result.get_type() == godot::Variant::DICTIONARY) {
				godot::Dictionary custom = result;
				godot::Array keys = custom.keys();
				for (int i = 0; i < keys.size(); i++) {
					payload[keys[i]] = custom[keys[i]];
				}
			}
		}

		_create_drag_ghost(source.element_id, source.ghost_builder);

		emit_signal("rml_drag_started",
			godot::String(source.element_id.c_str()), payload);

		return payload;
	}

	return {};
}

bool RmlContext::_can_drop_data(const godot::Vector2& p_at_position,
	const godot::Variant& /*p_data*/) const {

	if (_rml_context == nullptr || _drop_targets.empty()) return false;

	for (const auto& target : _drop_targets) {
		Rml::Element* el = _find_element(godot::String(target.element_id.c_str()));
		if (el == nullptr) continue;

		if (_point_in_element(el, p_at_position.x, p_at_position.y))
			return true;
	}

	return false;
}

void RmlContext::_drop_data(const godot::Vector2& p_at_position,
	const godot::Variant& p_data) {

	if (_rml_context == nullptr || _drop_targets.empty()) return;

	for (const auto& target : _drop_targets) {
		Rml::Element* el = _find_element(godot::String(target.element_id.c_str()));
		if (el == nullptr) continue;

		if (!_point_in_element(el, p_at_position.x, p_at_position.y)) continue;

		godot::String element_id(target.element_id.c_str());

		if (target.drop_handler.is_valid()) {
			target.drop_handler.call(element_id, p_data);
		}

		godot::Dictionary signal_data;
		if (p_data.get_type() == godot::Variant::DICTIONARY) {
			signal_data = p_data;
		}
		emit_signal("rml_drop_received", element_id, signal_data);
		return;
	}
}

Rml::String RmlContext::_build_ghost_rml(Rml::Element* el, int w, int h) {
	const auto& cv = el->GetComputedValues();

	auto fmt_color = [](Rml::Colourb c) -> std::string {
		char buf[32];
		snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%d)", c.red, c.green, c.blue, c.alpha);
		return buf;
	};

	auto fmt_px = [](float v) -> std::string {
		char buf[16];
		snprintf(buf, sizeof(buf), "%.0fpx", v);
		return buf;
	};

	std::string style;
	style += "display: block; overflow: hidden; ";
	style += "width: " + std::to_string(w) + "px; ";
	style += "height: " + std::to_string(h) + "px; ";
	style += "background-color: " + fmt_color(cv.background_color()) + "; ";
	style += "color: " + fmt_color(cv.color()) + "; ";
	style += "opacity: " + std::to_string(cv.opacity() * 0.8f) + "; ";
	style += "font-family: " + std::string(cv.font_family().c_str()) + "; ";
	style += "font-size: " + fmt_px(cv.font_size()) + "; ";

	float pt = cv.padding_top().value, pr = cv.padding_right().value;
	float pb = cv.padding_bottom().value, pl = cv.padding_left().value;
	if (pt > 0 || pr > 0 || pb > 0 || pl > 0) {
		style += "padding: " + fmt_px(pt) + " " + fmt_px(pr) + " "
			+ fmt_px(pb) + " " + fmt_px(pl) + "; ";
	}

	switch (cv.text_align()) {
		case Rml::Style::TextAlign::Center:  style += "text-align: center; "; break;
		case Rml::Style::TextAlign::Right:   style += "text-align: right; "; break;
		case Rml::Style::TextAlign::Justify: style += "text-align: justify; "; break;
		default: break;
	}

	float btw = cv.border_top_width(), brw = cv.border_right_width();
	float bbw = cv.border_bottom_width(), blw = cv.border_left_width();
	if (btw > 0) {
		style += "border-top-width: " + fmt_px(btw) + "; ";
		style += "border-top-color: " + fmt_color(cv.border_top_color()) + "; ";
	}
	if (brw > 0) {
		style += "border-right-width: " + fmt_px(brw) + "; ";
		style += "border-right-color: " + fmt_color(cv.border_right_color()) + "; ";
	}
	if (bbw > 0) {
		style += "border-bottom-width: " + fmt_px(bbw) + "; ";
		style += "border-bottom-color: " + fmt_color(cv.border_bottom_color()) + "; ";
	}
	if (blw > 0) {
		style += "border-left-width: " + fmt_px(blw) + "; ";
		style += "border-left-color: " + fmt_color(cv.border_left_color()) + "; ";
	}

	Rml::String inner_rml = el->GetInnerRML();

	return "<rml><head><style>body { margin: 0; padding: 0; }</style></head><body>"
		"<div style=\"" + Rml::String(style) + "\">" + inner_rml + "</div>"
		"</body></rml>";
}

void RmlContext::_create_drag_ghost(const std::string& source_element_id,
	const godot::Callable& ghost_builder) {

	Rml::Element* el = _find_element(godot::String(source_element_id.c_str()));
	if (el == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Drag ghost: source element not found");
		return;
	}

	Rml::Vector2f el_size = el->GetBox().GetSize(Rml::BoxArea::Border);
	int w = std::max(1, static_cast<int>(el_size.x));
	int h = std::max(1, static_cast<int>(el_size.y));
	godot::Vector2 size_vec(w, h);

	godot::String ghost_rml;
	if (ghost_builder.is_valid()) {
		godot::Variant result = ghost_builder.call(
			godot::String(source_element_id.c_str()), size_vec);
		if (result.get_type() == godot::Variant::STRING) {
			ghost_rml = result;
		}
	}

	if (ghost_rml.is_empty()) {
		ghost_rml = godot::String(_build_ghost_rml(el, w, h).c_str());
	}

	RmlContext* ghost = memnew(RmlContext);
	ghost->_context_name = "drag_ghost";
	ghost->set_custom_minimum_size(size_vec);
	ghost->set_size(size_vec);
	ghost->set_mouse_filter(MOUSE_FILTER_IGNORE);

	auto* manager = RmlGodot::RmlManager::get_singleton();
	manager->ensure_initialized();

	static int s_ghost_counter = 0;
	std::string ghost_name = "drag_ghost_" + std::to_string(s_ghost_counter++);
	ghost->_rml_context = Rml::CreateContext(Rml::String(ghost_name),
		Rml::Vector2i(w, h), &ghost->_render_interface);

	if (ghost->_rml_context == nullptr) {
		memdelete(ghost);
		godot::UtilityFunctions::push_warning("[RmlUi] Drag ghost: failed to create context");
		return;
	}
	ghost->_rml_context->SetDensityIndependentPixelRatio(_dp_ratio);

	for (const auto& [name, tex] : _render_interface.get_registered_textures()) {
		ghost->_render_interface.register_texture(name, tex);
	}

	Rml::ElementDocument* doc = ghost->_rml_context->LoadDocumentFromMemory(
		Rml::String(ghost_rml.utf8().get_data()));
	if (doc == nullptr) {
		memdelete(ghost);
		godot::UtilityFunctions::push_warning("[RmlUi] Drag ghost: document load failed");
		return;
	}

	doc->Show();
	ghost->_rml_context->Update();

	set_drag_preview(ghost);
}

// --- Phase 8b: Dev tools & extended document management ---

bool RmlContext::inject_stylesheet(const godot::String& rcss_string) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot inject stylesheet — context not initialized");
		return false;
	}

	Rml::String rcss(rcss_string.utf8().get_data());
	auto new_styles = Rml::Factory::InstanceStyleSheetString(rcss);
	if (!new_styles) {
		godot::UtilityFunctions::push_error("[RmlUi] Failed to parse injected stylesheet");
		return false;
	}

	int injected_count = 0;
	for (auto& ld : _loaded_documents) {
		if (ld.document == nullptr) continue;

		const Rml::StyleSheetContainer* existing = ld.document->GetStyleSheetContainer();
		if (existing != nullptr) {
			auto combined = existing->CombineStyleSheetContainer(*new_styles);
			ld.document->SetStyleSheetContainer(std::move(combined));
		} else {
			ld.document->SetStyleSheetContainer(new_styles);
		}
		injected_count++;
	}

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Stylesheet injected into ") +
		godot::String::num_int64(injected_count) + godot::String(" document(s)"));
	return injected_count > 0;
}

bool RmlContext::unload_document(const godot::String& path) {
	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_warning("[RmlUi] Cannot unload document — context not initialized");
		return false;
	}

	std::string path_str(path.utf8().get_data());

	auto it = std::find_if(_loaded_documents.begin(), _loaded_documents.end(),
		[&](const LoadedDocument& ld) { return ld.path == path_str; });

	if (it == _loaded_documents.end()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Document not tracked for unload: ") + path);
		return false;
	}

	if (it->document != nullptr) {
		_rml_context->UnloadDocument(it->document);
	}

	_loaded_documents.erase(it);

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Document unloaded: ") + path);
	return true;
}

godot::Dictionary RmlContext::get_context_info() const {
	godot::Dictionary info;

	if (_rml_context == nullptr) {
		info["initialized"] = false;
		return info;
	}

	info["initialized"] = true;
	info["name"] = _context_name;
	info["dp_ratio"] = _dp_ratio;

	auto dims = _rml_context->GetDimensions();
	info["width"] = dims.x;
	info["height"] = dims.y;
	info["num_documents"] = _rml_context->GetNumDocuments();
	info["num_data_models"] = static_cast<int>(_data_models.size());
	info["num_listeners"] = static_cast<int>(_listener_records.size());
	info["num_loaded_paths"] = static_cast<int>(_loaded_documents.size());

	info["num_geometry"] = static_cast<int>(_render_interface.get_geometry_count());
	info["num_textures"] = static_cast<int>(_render_interface.get_texture_count());
	info["num_filters"] = static_cast<int>(_render_interface.get_filter_count());
	info["num_draw_commands"] = static_cast<int>(_render_interface.get_draw_command_count());

	return info;
}

// --- Private: Input forwarding ---

static int godot_button_to_rml(godot::MouseButton button) {
	switch (button) {
		case godot::MOUSE_BUTTON_LEFT:   return 0;
		case godot::MOUSE_BUTTON_RIGHT:  return 1;
		case godot::MOUSE_BUTTON_MIDDLE: return 2;
		default: return 3;
	}
}

static int godot_modifiers_to_rml(const godot::Ref<godot::InputEvent>& event) {
	int mod = 0;
	auto* key_event = godot::Object::cast_to<godot::InputEventWithModifiers>(event.ptr());
	if (key_event == nullptr) return mod;

	if (key_event->is_ctrl_pressed())  mod |= Rml::Input::KM_CTRL;
	if (key_event->is_shift_pressed()) mod |= Rml::Input::KM_SHIFT;
	if (key_event->is_alt_pressed())   mod |= Rml::Input::KM_ALT;
	if (key_event->is_meta_pressed())  mod |= Rml::Input::KM_META;
	return mod;
}

void RmlContext::_forward_mouse_event(const godot::Ref<godot::InputEvent>& event) {
	auto* motion = godot::Object::cast_to<godot::InputEventMouseMotion>(event.ptr());
	if (motion != nullptr) {
		godot::Vector2 pos = motion->get_position();
		_rml_context->ProcessMouseMove(
			static_cast<int>(pos.x), static_cast<int>(pos.y),
			godot_modifiers_to_rml(event));
		return;
	}

	const auto* button = godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr());

	if (button == nullptr)
		return;

	const godot::Vector2 pos = button->get_position();
	_rml_context->ProcessMouseMove(
		static_cast<int>(pos.x), static_cast<int>(pos.y),
		godot_modifiers_to_rml(event));

	// Scroll wheel
	if (button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_UP && button->is_pressed()) {
		_rml_context->ProcessMouseWheel(Rml::Vector2f(0, -1), godot_modifiers_to_rml(event));
		return;
	}
	if (button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_DOWN && button->is_pressed()) {
		_rml_context->ProcessMouseWheel(Rml::Vector2f(0, 1), godot_modifiers_to_rml(event));
		return;
	}

	const int rml_button = godot_button_to_rml(button->get_button_index());
	if (button->is_pressed()) {
		_rml_context->ProcessMouseButtonDown(rml_button, godot_modifiers_to_rml(event));
	} else {
		_rml_context->ProcessMouseButtonUp(rml_button, godot_modifiers_to_rml(event));
	}


}

void RmlContext::_forward_key_event(const godot::Ref<godot::InputEvent>& event) {
	auto* key = godot::Object::cast_to<godot::InputEventKey>(event.ptr());
	if (key == nullptr) return;

	// Minimal key mapping — extend as needed.
	Rml::Input::KeyIdentifier rml_key = Rml::Input::KI_UNKNOWN;

	//we could do here static constexpr mapping to keys, similar to what I do for my game engine.
	// static constexpr std::array<Rml::Input::KeyIdentifier, 26> s_key_map = {
	// 	Rml::Input::KI_A, Rml::Input::KI_B, Rml::Input::KI_C, Rml::Input::KI_D, Rml::Input::KI_E,
	// 	Rml::Input::KI_F, Rml::Input::KI_G, Rml::Input::KI_H, Rml::Input::KI_I, Rml::Input::KI_J,
	// 	Rml::Input::KI_K, Rml::Input::KI_L, Rml::Input::KI_M, Rml::Input::KI_N, Rml::Input::KI_O,
	// 	Rml::Input::KI_P, Rml::Input::KI_Q, Rml::Input::KI_R, Rml::Input::KI_S, Rml::Input::KI_T,
	// 	Rml::Input::KI_U, Rml::Input::KI_V, Rml::Input::KI_W, Rml::Input::KI_X, Rml::Input::KI_Y,
	// 	Rml::Input::KI_Z
	// };

	//like map that is queried with godot keys -> returns the KI relevant key

	// not quite like the above it needs to be but it is still a simple mapping that covers all letters and digits.
	// like direct mapping to godot -> Rml. it needs double map. and we cannot do that directly godot KEY backspace is 4194308 large number.
	auto keycode = key->get_keycode();
	if (keycode >= godot::KEY_A && keycode <= godot::KEY_Z) {
		rml_key = static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_A + (static_cast<int>(keycode) - static_cast<int>(godot::KEY_A)));
	} else if (keycode >= godot::KEY_0 && keycode <= godot::KEY_9) {
		rml_key = static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_0 + (static_cast<int>(keycode) - static_cast<int>(godot::KEY_0)));
	} else if (keycode == godot::KEY_ENTER || keycode == godot::KEY_KP_ENTER) {
		rml_key = Rml::Input::KI_RETURN;
	} else if (keycode == godot::KEY_BACKSPACE) {
		rml_key = Rml::Input::KI_BACK;
	} else if (keycode == godot::KEY_TAB) {
		rml_key = Rml::Input::KI_TAB;
	} else if (keycode == godot::KEY_ESCAPE) {
		rml_key = Rml::Input::KI_ESCAPE;
	} else if (keycode == godot::KEY_F8) {
		if (key->is_pressed()) {
			static bool s_debugger_inited = false;
			if (!s_debugger_inited) {
				Rml::Debugger::Initialise(_rml_context);
				s_debugger_inited = true;
			}
			Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
		}
		return;
	}

	const int modifiers = godot_modifiers_to_rml(event);

	if (key->is_pressed()) {
		_rml_context->ProcessKeyDown(rml_key, modifiers);

		// Forward printable characters for text input.
		if (key->get_unicode() > 0) {
			_rml_context->ProcessTextInput(static_cast<Rml::Character>(key->get_unicode()));
		}
	} else {
		_rml_context->ProcessKeyUp(rml_key, modifiers);
	}
}

} // namespace RmlGodot
