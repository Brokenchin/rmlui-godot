// RmlContext — lifecycle, rendering and configuration.
// The class implementation is split across translation units:
//   RmlContext.cpp          lifecycle, _draw pipeline, context info
//   RmlContextDocuments.cpp documents, fonts, stylesheets
//   RmlContextData.cpp      data models, variables, arrays
//   RmlContextDom.cpp       elements, events, textures, decorators
//   RmlContextInput.cpp     input forwarding, navigation, drag & drop
#include "RmlContext.hpp"
#include "RmlManager.hpp"
#include "RmlElementHandle.hpp"
#include "GodotEventListener.hpp"
#include "GodotFontInterface.hpp"
#include "GodotScriptDocument.hpp"

#include <algorithm>
#include <utility>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StyleSheetContainer.h>
#include <RmlUi/Debugger.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/viewport.hpp>
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
	_update_unhandled_input_processing();
	// Click-to-focus: gui keyboard events (text inputs, key forwarding) only
	// reach a focused Control, and the default focus_mode is NONE — keyboard
	// interaction with documents silently required focus nobody could give.
	if (get_focus_mode() == godot::Control::FOCUS_NONE) {
		set_focus_mode(godot::Control::FOCUS_CLICK);
	}
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
	// rasterization already uses them.
	auto& font_iface = manager->get_font_interface();
	font_iface.set_text_render_mode(_text_render_mode);
	font_iface.set_hinting(_font_hinting);
	font_iface.set_font_antialiasing(_font_antialiasing);
	font_iface.set_subpixel_positioning(_font_subpixel);
	font_iface.set_font_oversampling(_font_oversampling);
	font_iface.set_pixel_snap(_font_pixel_snap);
	font_iface.set_layout_mode(_font_layout_mode);

	for (int i = 0; i < _font_paths.size(); i++) {
		load_font_face(_font_paths[i]);
	}

	// In the editor, stand up mock data models BEFORE the document loads so
	// data-model/data-for/{{ }} render in the 2D viewport without the game
	// running (runtime models are script-bound and can't exist here).
	auto* engine = godot::Engine::get_singleton();
	if (engine != nullptr && engine->is_editor_hint() && !_editor_mock_data.is_empty()) {
		_apply_editor_mock_data();
	}

	if (!_document_path.is_empty()) {
		// Deferred: children _ready before parents, so a direct load here runs
		// BEFORE the owning scene's script can create data models / register
		// custom elements — bindings would fail and need a reload. Deferring
		// to after the whole _ready cascade lets the natural pattern work:
		// create models in _ready, the document binds them on load.
		// (Anything needing the loaded document should await one frame.)
		call_deferred("load_document", _document_path);
	}
}

void RmlContext::_apply_editor_mock_data() {
	godot::Array model_names = _editor_mock_data.keys();
	for (int i = 0; i < model_names.size(); i++) {
		const godot::String model_name = model_names[i];
		godot::Dictionary vars = _editor_mock_data[model_name];
		if (!create_data_model(model_name)) continue;
		godot::Array var_names = vars.keys();
		for (int j = 0; j < var_names.size(); j++) {
			const godot::String var_name = var_names[j];
			godot::Variant value = vars[var_name];
			if (value.get_type() == godot::Variant::ARRAY) {
				bind_data_array(model_name, var_name, value);
			} else {
				bind_data_variable(model_name, var_name, value);
			}
		}
	}
}

void RmlContext::_process(double /*delta*/) {
	if (_rml_context == nullptr) return;

	_sync_dimensions();
	_rml_context->Update();

	// Hover bridge: detect hover-chain changes after the Update settled them and
	// push rml_element_hovered / rml_element_unhovered to any external overlay.
	_update_hover_tracking();

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager && manager->is_initialized()) {
		int fv = manager->get_font_interface().get_global_version();
		if (fv != _last_font_version) {
			_last_font_version = fv;
			_render_dirty = true;
		}
	}

	if (_render_dirty || _rml_context->GetNextUpdateDelay() == 0) {
		_render_dirty = false;
		queue_redraw();
	}
}

void RmlContext::_draw() {
	if (_rml_context == nullptr) return;

	auto* rs = godot::RenderingServer::get_singleton();
	if (rs == nullptr) return;

	// Free previous frame's canvas items and deferred geometry BEFORE Render()
	// so old RIDs are removed from the rendering tree before we release meshes.
	_free_scissor_items();
	_free_layer_items();
	_render_interface.flush_deferred_releases();

	_render_interface.clear_draw_commands();
	_rml_context->Render();

	const auto& commands = _render_interface.get_draw_commands();

	if (!_active_material.is_valid()) return;
	godot::RID mat_rid = _active_material->get_rid();

	if (_gpu_scissor) _ensure_scissor_material();
	const bool use_gpu = _gpu_scissor && _scissor_material.is_valid();
	godot::RID scissor_mat_rid = use_gpu ? _scissor_material->get_rid() : godot::RID();
	godot::Vector2 global_pos = get_global_position();

	ClipResult clip_buf;

	using CmdType = RmlGodot::GodotRenderInterface::CommandType;

	godot::Vector2 ctrl_size = get_size();

	// Text glyphs get an explicit filter (text_filtering_mode property);
	// everything else stays on RS DEFAULT = the project setting. The node's
	// texture_filter is deliberately NOT consulted — raw RS items skip
	// node-tree inheritance anyway, and text crispness should not depend on
	// project-wide or per-node image filtering.
	const auto text_filter = (_text_filtering_mode == 1)
		? godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR
		: godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;

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
	godot::RenderingServer::CanvasItemTextureFilter run_filter =
		godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
	int run_draw_index = 0;

	auto invalidate_run = [&]() { run_item = godot::RID(); };

	auto target_for = [&](godot::RID parent, godot::RID material,
			bool scissored, const godot::Rect2& rect,
			godot::RenderingServer::CanvasItemTextureFilter filter) -> godot::RID {
		if (run_item.is_valid() && run_parent == parent && run_material == material &&
			run_scissored == scissored && run_filter == filter &&
			(!scissored || run_rect == rect)) {
			return run_item;
		}
		godot::RID item = rs->canvas_item_create();
		rs->canvas_item_set_parent(item, parent);
		rs->canvas_item_set_material(item, material);
		rs->canvas_item_set_default_texture_filter(item, filter);
		rs->canvas_item_set_draw_index(item, run_draw_index++);
		if (material == scissor_mat_rid) {
			godot::Vector4 rv = scissored
				? godot::Vector4(rect.position.x + global_pos.x, rect.position.y + global_pos.y, rect.size.x, rect.size.y)
				: godot::Vector4(-1000000.0f, -1000000.0f, 2000000.0f, 2000000.0f);
			rs->canvas_item_set_instance_shader_parameter(item, "scissor_rect", rv);
		}
		_scissor_items.push_back(item);
		run_item = item;
		run_parent = parent;
		run_material = material;
		run_scissored = scissored;
		run_rect = rect;
		run_filter = filter;
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

			// Glyph-atlas draws use the explicit text filter; everything else
			// follows the project default.
			const auto cmd_filter = _render_interface.is_generated_texture(cmd.texture)
				? text_filter
				: godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;

			bool fully_inside = (mesh_left >= clip_rect.position.x &&
				mesh_top >= clip_rect.position.y &&
				mesh_right <= clip_rect.position.x + clip_rect.size.x &&
				mesh_bottom <= clip_rect.position.y + clip_rect.size.y);

			bool needs_scissor = cmd.scissor_enabled && !fully_inside;

			// GPU scissor only applies to ordinary geometry: a decorator shader
			// has its own material with no scissor uniform, so it always CPU-clips.
			bool gpu_path = use_gpu && !is_shader;

			if (gpu_path) {
				godot::RID target = target_for(draw_target, scissor_mat_rid, needs_scissor, clip_rect, cmd_filter);
				rs->canvas_item_add_mesh(target, mesh->get_rid(), xform,
					godot::Color(1, 1, 1, 1), tex_rid);
			} else if (needs_scissor) {
				const auto* raw = _render_interface.get_raw_geometry(cmd.geometry);
				if (raw && _clip_mesh_to_rect(*raw, xform, clip_rect, clip_buf)) {
					godot::RID target = target_for(draw_target, geo_material, false, clip_rect, cmd_filter);
					rs->canvas_item_add_triangle_array(target,
						clip_buf.indices, clip_buf.positions, clip_buf.colors,
						clip_buf.uvs, godot::PackedInt32Array(),
						godot::PackedFloat32Array(), tex_rid);
				}
			} else {
				godot::RID target = target_for(draw_target, geo_material, false, clip_rect, cmd_filter);
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
			_render_dirty = true;
		}
	} else if (p_what == godot::Control::NOTIFICATION_MOUSE_EXIT) {
		// Godot stops delivering motion once the cursor leaves the Control, so
		// RmlUi never learns the mouse left and the hover chain would stay stuck.
		// Clear it explicitly — _process then emits the pending unhover.
		if (_rml_context != nullptr) {
			_rml_context->ProcessMouseLeave();
			_render_dirty = true;
		}
	} else if (p_what == godot::Node::NOTIFICATION_ENTER_TREE) {
		// Re-entering the tree (editor scene-tab switch, reparenting): the
		// visuals were freed on exit — repaint from the still-alive context.
		_render_dirty = true;
	} else if (p_what == godot::Node::NOTIFICATION_EXIT_TREE) {
		// Only drop the visual canvas items here. The Rml context, documents,
		// and data models survive so the node can re-enter the tree intact —
		// the editor detaches inactive scene tabs, and games reparent UI.
		// Full teardown happens in the destructor.
		auto* rs = godot::RenderingServer::get_singleton();
		if (rs != nullptr) {
			_free_scissor_items();
			_free_layer_items();
			_render_interface.flush_deferred_releases();
		}
	}
}

void RmlContext::_create_context() {
	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager == nullptr || !manager->is_initialized()) return;
	if (_rml_context != nullptr) return;

	godot::Vector2 size = get_size();
	if (size.x < 1 || size.y < 1) {
		size = godot::Vector2(800, 600);
	}

	// Context names must be unique process-wide. Multiple instances sharing a
	// configured name (e.g. several open scenes plus the editor preview, all
	// "default") get a unique suffix appended.
	Rml::String name(_context_name.utf8().get_data());
	if (Rml::GetContext(name) != nullptr) {
		name += Rml::String("_") +
			Rml::String(godot::String::num_uint64(get_instance_id()).utf8().get_data());
	}
	_rml_context = Rml::CreateContext(name,
		Rml::Vector2i(static_cast<int>(size.x), static_cast<int>(size.y)),
		&_render_interface);

	if (_rml_context == nullptr) {
		godot::UtilityFunctions::push_error("[RmlUi] Failed to create context");
		return;
	}

	_rml_context->SetDensityIndependentPixelRatio(_dp_ratio);
	manager->register_context_node(_rml_context, this);

	godot::UtilityFunctions::print(
		godot::String("[RmlUi] Context created: ") + _context_name +
		godot::String(" (") + godot::String::num_int64(static_cast<int64_t>(size.x)) +
		godot::String("x") + godot::String::num_int64(static_cast<int64_t>(size.y)) +
		godot::String(")"));
}

void RmlContext::_destroy_context() {
	if (_rml_context == nullptr) return;

	auto* manager = RmlGodot::RmlManager::get_singleton();
	if (manager) {
		manager->unregister_context_node(_rml_context);
	}
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
	_last_hovered_id.clear();

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

// --- Public: auto-configuration ---

void RmlContext::set_document_path(const godot::String& path) {
	if (_document_path == path) return;
	const godot::String old = _document_path;
	_document_path = path;

	// Live change (inspector edit or runtime assignment) — swap the document.
	// During scene instantiation _rml_context is still null and _ready()
	// performs the initial load.
	if (_rml_context != nullptr) {
		if (!old.is_empty()) {
			// Only unload if the old path actually loaded (it may have been
			// missing/broken) — unload_document warns about untracked paths.
			std::string old_str(old.utf8().get_data());
			bool old_loaded = std::any_of(_loaded_documents.begin(), _loaded_documents.end(),
				[&](const LoadedDocument& ld) { return ld.path == old_str; });
			if (old_loaded) {
				unload_document(old);
			}
		}
		if (!path.is_empty()) {
			load_document(path);
		}
	}
	update_configuration_warnings();

	// Rebuild any open inspector so path-dependent custom controls
	// (Edit/Create buttons) reflect the new document immediately.
	auto* engine = godot::Engine::get_singleton();
	if (engine != nullptr && engine->is_editor_hint()) {
		notify_property_list_changed();
	}
}

void RmlContext::set_font_paths(const godot::PackedStringArray& paths) {
	const godot::PackedStringArray old = _font_paths;
	_font_paths = paths;

	// Live change: load any newly added faces. Removed paths stay loaded —
	// RmlUi has no per-face unload.
	if (_rml_context != nullptr) {
		for (int i = 0; i < paths.size(); i++) {
			if (!old.has(paths[i])) {
				load_font_face(paths[i]);
			}
		}
		_render_dirty = true;
	}
	update_configuration_warnings();
}

godot::PackedStringArray RmlContext::_get_configuration_warnings() const {
	godot::PackedStringArray warnings;

	if (RmlGodot::RmlManager::get_singleton() == nullptr) {
		warnings.append("RmlManager singleton not available — is the rmlui-godot GDExtension loaded?");
		return warnings;
	}

	if (_document_path.is_empty()) {
		warnings.append("No document_path set. Set Auto-Configuration > Document Path to load an .rml document (script-driven load_document() calls don't run in the editor).");
	} else if (!godot::FileAccess::file_exists(_document_path)) {
		warnings.append(godot::String("Document file not found: ") + _document_path);
	}

	for (int i = 0; i < _font_paths.size(); i++) {
		if (!godot::FileAccess::file_exists(_font_paths[i])) {
			warnings.append(godot::String("Font file not found: ") + _font_paths[i]);
		}
	}

	if (_font_paths.is_empty()) {
		auto* manager = RmlGodot::RmlManager::get_singleton();
		bool has_global_fonts = manager->is_initialized() && manager->get_loaded_fonts().size() > 0;
		if (!has_global_fonts) {
			warnings.append("No fonts configured (font_paths is empty and no global fonts are loaded) — text will not render.");
		}
	}

	return warnings;
}

// --- Public: dp_ratio ---

void RmlContext::set_dp_ratio(float ratio) {
	_dp_ratio = ratio;
	if (_rml_context != nullptr) {
		_rml_context->SetDensityIndependentPixelRatio(ratio);
	}
}

void RmlContext::set_gpu_scissor(bool enabled) {
	if (_gpu_scissor == enabled) return;
	_gpu_scissor = enabled;
	_render_dirty = true;
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

} // namespace RmlGodot
