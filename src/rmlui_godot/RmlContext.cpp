// RmlContext — lifecycle, rendering and configuration.
// The class implementation is split across translation units:
//   RmlContext.cpp          lifecycle, _draw pipeline, context info
//   RmlContextDocuments.cpp documents, fonts, stylesheets
//   RmlContextData.cpp      data models, variables, arrays
//   RmlContextDom.cpp       elements, events, textures, decorators
//   RmlContextEmbed.cpp     embedded sub-documents (<embed-doc>, issue #56)
//   RmlContextInput.cpp     input forwarding, navigation, drag & drop
#include "RmlContext.hpp"
#include "RmlManager.hpp"
#include "RmlElementHandle.hpp"
#include "GodotEventListener.hpp"
#include "GodotFontInterface.hpp"
#include "GodotScriptDocument.hpp"

#include <algorithm>
#include <chrono>
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

void RmlContext::_process(double delta) {
	if (_rml_context == nullptr) return;

	// Snapshot the input-forwarding cost accumulated since the previous _process
	// (events arrive in _gui_input, possibly several per frame).
	_last_input_us = _input_us_accum;
	_last_input_events = _input_events_accum;
	_input_us_accum = 0;
	_input_events_accum = 0;

	const auto t_sync0 = std::chrono::steady_clock::now();
	_sync_dimensions();
	const auto t_upd0 = std::chrono::steady_clock::now();
	_last_sync_us = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(t_upd0 - t_sync0).count());
	_rml_context->Update();
	const auto t_upd1 = std::chrono::steady_clock::now();
	_last_update_us = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(t_upd1 - t_upd0).count());

	// Issue #56: nested embedded documents are not laid out by Context::Update's
	// root-only layout loop, so reflow any whose internals changed (and reflow the
	// parent if an embed's outer size changed). Runs after Update so it observes
	// the settled tree.
	_update_embed_layout();
	_last_embed_update_us = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - t_upd1).count());

	// Hover bridge: detect hover-chain changes after the Update settled them and
	// push rml_element_hovered / rml_element_unhovered to any external overlay.
	const auto t_hov0 = std::chrono::steady_clock::now();
	_update_hover_tracking();
	_last_hover_us = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - t_hov0).count());

	// Game-first per-frame tick (issue #41): lets time-based gestures — long-
	// press, double-tap timeout, hold-to-charge — run from a document <script>
	// without a node's _process. Fires after Update so the handler observes a
	// settled layout and hover chain; any DOM it mutates marks the frame dirty
	// through the usual mutation APIs and is picked up by the redraw gate below.
	if (_input_tick.is_valid()) {
		_input_tick.call(delta);
  }
	// Issue #37: keep the active drag ghost (if any) pinned under the cursor.
	if (_ghost_layer != nullptr) {
		_update_ghost_position();
	}

	// Issue #47: a passive mouse-move (no _render_dirty set by _gui_input) only
	// matters visually when the hover chain changes. The deepest hovered
	// element's ancestry IS the hover chain, so a change in that element means
	// some element gained or lost :hover and we must repaint. Compared by pointer
	// identity only — never dereferenced — so a value left dangling by a
	// since-freed element is harmless (worst case one missed frame, corrected on
	// the next move).
	Rml::Element* hover = _rml_context->GetHoverElement();
	if (hover != _last_hover_element) {
		_last_hover_element = hover;
		_render_dirty = true;
	}

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

	_frame_stats = FrameStats{};
	const auto t_render0 = std::chrono::steady_clock::now();

	// Release meshes retired two frames ago. Their canvas-item references were
	// dropped when the owning slot last changed (canvas_item_clear), so freeing
	// the RIDs now is safe. Canvas items are NOT torn down here — they persist
	// across frames and are reconciled at the end of this function.
	_render_interface.flush_deferred_releases();

	_render_interface.clear_draw_commands();
	_rml_context->Render();

	const auto t_build0 = std::chrono::steady_clock::now();
	_frame_stats.render_us = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(t_build0 - t_render0).count());

	const auto& commands = _render_interface.get_draw_commands();
	_frame_stats.draw_commands = static_cast<uint32_t>(commands.size());

	// Build this frame's slots into the non-current ping-pong buffer, reusing
	// its Slot storage (and each Slot's prims vector) so the steady state
	// performs no per-frame heap allocation. `nb[i].desc` is the i-th slot;
	// new_slot() resets and hands out the next one.
	std::vector<Slot>& nb = _slots_buf[1 - _slots_cur];
	size_t used = 0;
	auto new_slot = [&](SlotDesc::Kind kind) -> int {
		if (used >= nb.size()) nb.emplace_back();
		SlotDesc& d = nb[used].desc;
		d.kind = kind;
		d.parent = -1;
		d.material = godot::RID();
		d.filter = godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
		d.draw_index = -1;
		d.group_mode = -1;
		d.modulate_set = false;
		d.modulate = godot::Color(1, 1, 1, 1);
		d.set_scissor_param = false;
		d.scissor_param = godot::Vector4();
		d.prims.clear(); // retains capacity
		return static_cast<int>(used++);
	};

	if (!_active_material.is_valid()) {
		_reconcile_slots(0); // frees everything from the last frame
		return;
	}
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
	const int text_filter = (_text_filtering_mode == 1)
		? godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR
		: godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;

	// Root slot (index 0): a child canvas item of this control's canvas item.
	new_slot(SlotDesc::ROOT);
	nb[0].desc.material = mat_rid;

	// Layer/draw-target tracking by slot index (was canvas-item RIDs).
	std::vector<int> layer_stack;
	layer_stack.push_back(0);
	int draw_target_index = 0;

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
	int cur_run = -1;
	int run_parent = -1;
	godot::RID run_material;
	bool run_scissored = false;
	godot::Rect2 run_rect;
	int run_filter = godot::RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
	int run_draw_index = 0;

	auto invalidate_run = [&]() { cur_run = -1; };

	auto target_for = [&](int parent_idx, godot::RID material,
			bool scissored, const godot::Rect2& rect, int filter) -> int {
		if (cur_run != -1 && run_parent == parent_idx && run_material == material &&
			run_scissored == scissored && run_filter == filter &&
			(!scissored || run_rect == rect)) {
			return cur_run;
		}
		int idx = new_slot(SlotDesc::RUN);
		SlotDesc& d = nb[idx].desc;
		d.parent = parent_idx;
		d.material = material;
		d.filter = filter;
		d.draw_index = run_draw_index++;
		if (material == scissor_mat_rid) {
			d.set_scissor_param = true;
			d.scissor_param = scissored
				? godot::Vector4(rect.position.x + global_pos.x, rect.position.y + global_pos.y, rect.size.x, rect.size.y)
				: godot::Vector4(-1000000.0f, -1000000.0f, 2000000.0f, 2000000.0f);
		}
		cur_run = idx;
		run_parent = parent_idx;
		run_material = material;
		run_scissored = scissored;
		run_rect = rect;
		run_filter = filter;
		return cur_run;
	};

	for (int ci = 0; ci < static_cast<int>(commands.size()); ci++) {
		const auto& cmd = commands[ci];

		switch (cmd.type) {

		case CmdType::PUSH_LAYER: {
			invalidate_run();
			int parent_idx = layer_stack.back();
			int gi = new_slot(SlotDesc::GROUP);
			SlotDesc& d = nb[gi].desc;
			d.parent = parent_idx;
			d.material = mat_rid;
			d.group_mode = godot::RenderingServer::CANVAS_GROUP_MODE_TRANSPARENT;
			layer_stack.push_back(gi);
			draw_target_index = gi;
			break;
		}

		case CmdType::COMPOSITE_LAYERS: {
			invalidate_run();
			if (layer_stack.size() < 2) break;
			int cur = layer_stack.back();

			float opacity = 1.0f;
			for (auto filter_handle : cmd.filters) {
				const auto* filter = _render_interface.get_filter(filter_handle);
				if (filter == nullptr) continue;
				if (filter->type == RmlGodot::GodotRenderInterface::FilterData::Type::OPACITY) {
					opacity *= filter->value;
				}
			}

			if (opacity < 1.0f) {
				nb[cur].desc.modulate_set = true;
				nb[cur].desc.modulate = godot::Color(1.0f, 1.0f, 1.0f, opacity);
			}
			break;
		}

		case CmdType::POP_LAYER: {
			invalidate_run();
			if (layer_stack.size() > 1) {
				layer_stack.pop_back();
				draw_target_index = layer_stack.back();
			}
			break;
		}

		case CmdType::ENABLE_CLIP_MASK: {
			invalidate_run();
			if (layer_stack.size() < 2) break;
			int cur = layer_stack.back();
			nb[cur].desc.group_mode = cmd.clip_mask_enabled
				? godot::RenderingServer::CANVAS_GROUP_MODE_CLIP_AND_DRAW
				: godot::RenderingServer::CANVAS_GROUP_MODE_TRANSPARENT;
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

			// The clip-mask quad must obey the active scissor exactly like ordinary
			// geometry (issue #61 render half). A border-radius element clips its
			// content with a clip mask instead of the scissor; when it scrolls or lays
			// out past an `overflow` ancestor its content is scissor-culled, but the
			// rounded mask quad (drawn with the white texture) used to be emitted
			// unconditionally and leaked outside the embed as a white square. Cull the
			// mask when fully outside the scissor, and clip it to the edge when it
			// straddles — keeping it paired with the content it masks.
			const godot::AABB mask_aabb = mask_mesh->get_aabb();
			const godot::Vector2 mask_origin = xform.get_origin();
			const float mask_l = mask_origin.x + static_cast<float>(mask_aabb.position.x);
			const float mask_t = mask_origin.y + static_cast<float>(mask_aabb.position.y);
			const float mask_r = mask_l + static_cast<float>(mask_aabb.size.x);
			const float mask_b = mask_t + static_cast<float>(mask_aabb.size.y);
			godot::Rect2 mask_clip(0, 0, ctrl_size.x, ctrl_size.y);
			if (cmd.scissor_enabled) {
				mask_clip = mask_clip.intersection(godot::Rect2(cmd.scissor_rect));
			}
			if (mask_clip.size.x <= 0 || mask_clip.size.y <= 0) break;
			if (mask_r <= mask_clip.position.x || mask_l >= mask_clip.position.x + mask_clip.size.x ||
				mask_b <= mask_clip.position.y || mask_t >= mask_clip.position.y + mask_clip.size.y) {
				break; // fully outside the scissor — cull (its content is culled too)
			}

			const bool mask_fully_inside =
				(mask_l >= mask_clip.position.x && mask_t >= mask_clip.position.y &&
					mask_r <= mask_clip.position.x + mask_clip.size.x &&
					mask_b <= mask_clip.position.y + mask_clip.size.y);

			godot::Ref<godot::Texture2D> tex = _render_interface.get_texture_or_white(0);
			godot::RID tex_rid = tex.is_valid() ? tex->get_rid() : godot::RID();

			// Clip-mask geometry draws directly into the group item (it must paint
			// before the group's children); recorded as a prim of that slot.
			SlotPrim p;
			p.geo_handle = static_cast<uintptr_t>(cmd.geometry);
			p.tex_handle = 0;
			p.xform = xform;
			p.tex_rid = tex_rid;
			if (cmd.scissor_enabled && !mask_fully_inside) {
				// Straddles the scissor edge: clip the mask to it (CPU) so the part
				// outside the overflow region doesn't paint as a white sliver.
				const auto* raw = _render_interface.get_raw_geometry(cmd.geometry);
				if (!(raw && _clip_mesh_to_rect(*raw, xform, mask_clip, clip_buf))) break;
				p.kind = SlotPrim::TRI_ARRAY;
				p.clip_rect = mask_clip;
			} else {
				p.kind = SlotPrim::MESH;
				p.mesh_rid = mask_mesh->get_rid();
			}
			nb[draw_target_index].desc.prims.push_back(std::move(p));
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

			draw_target_index = layer_stack.back();

			godot::Ref<godot::Texture2D> draw_tex = _render_interface.get_texture_or_white(cmd.texture);
			godot::RID tex_rid = draw_tex.is_valid() ? draw_tex->get_rid() : godot::RID();

			// Glyph-atlas draws use the explicit text filter; everything else
			// follows the project default.
			const int cmd_filter = _render_interface.is_generated_texture(cmd.texture)
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
				int target = target_for(draw_target_index, scissor_mat_rid, needs_scissor, clip_rect, cmd_filter);
				SlotPrim p;
				p.kind = SlotPrim::MESH;
				p.geo_handle = static_cast<uintptr_t>(cmd.geometry);
				p.tex_handle = static_cast<uintptr_t>(cmd.texture);
				p.xform = xform;
				p.mesh_rid = mesh->get_rid();
				p.tex_rid = tex_rid;
				nb[target].desc.prims.push_back(std::move(p));
			} else if (needs_scissor) {
				// CPU clip: validate it produces output now (so an empty clip
				// skips the draw exactly as before); the clipped triangle array
				// is recomputed in _apply_slot only when the slot actually changes.
				_frame_stats.tri_clips++;
				const auto* raw = _render_interface.get_raw_geometry(cmd.geometry);
				if (raw && _clip_mesh_to_rect(*raw, xform, clip_rect, clip_buf)) {
					int target = target_for(draw_target_index, geo_material, false, clip_rect, cmd_filter);
					SlotPrim p;
					p.kind = SlotPrim::TRI_ARRAY;
					p.geo_handle = static_cast<uintptr_t>(cmd.geometry);
					p.tex_handle = static_cast<uintptr_t>(cmd.texture);
					p.xform = xform;
					p.clip_rect = clip_rect;
					p.tex_rid = tex_rid;
					nb[target].desc.prims.push_back(std::move(p));
				}
			} else {
				int target = target_for(draw_target_index, geo_material, false, clip_rect, cmd_filter);
				SlotPrim p;
				p.kind = SlotPrim::MESH;
				p.geo_handle = static_cast<uintptr_t>(cmd.geometry);
				p.tex_handle = static_cast<uintptr_t>(cmd.texture);
				p.xform = xform;
				p.mesh_rid = mesh->get_rid();
				p.tex_rid = tex_rid;
				nb[target].desc.prims.push_back(std::move(p));
			}
			break;
		}

		} // switch
	}

	const auto t_rec0 = std::chrono::steady_clock::now();
	_frame_stats.build_us = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(t_rec0 - t_build0).count());
	_frame_stats.slots_used = static_cast<uint32_t>(used);

	_reconcile_slots(used);

	_frame_stats.reconcile_us = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - t_rec0).count());
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
	} else if (p_what == godot::Node::NOTIFICATION_DRAG_END) {
		// Issue #37: the drag finished (dropped or canceled — this fires for both,
		// propagated to every node) — tear down the ghost's CanvasLayer.
		_destroy_active_ghost();
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
			_free_all_slots();
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
	_last_hover_element = nullptr;

	// Issue #56: embedded documents are children of the parent documents unloaded
	// below (and torn down with the context), so just drop our tracking here.
	_embeds.clear();

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

	_free_all_slots();

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
		// SetDimensions re-evaluates only top-level documents' media queries —
		// nested embeds are skipped, so re-dirty their @media here (issue #56).
		_redirty_embeds_media();
	}
}

bool RmlContext::_prim_equal(const SlotPrim& a, const SlotPrim& b) {
	if (a.kind != b.kind || a.geo_handle != b.geo_handle || a.tex_handle != b.tex_handle)
		return false;
	if (a.xform != b.xform || a.modulate != b.modulate)
		return false;
	if (a.kind == SlotPrim::TRI_ARRAY && a.clip_rect != b.clip_rect)
		return false;
	return true;
}

bool RmlContext::_desc_equal(const SlotDesc& a, const SlotDesc& b) {
	if (a.kind != b.kind || a.parent != b.parent || a.material != b.material ||
		a.filter != b.filter || a.draw_index != b.draw_index ||
		a.group_mode != b.group_mode || a.modulate_set != b.modulate_set ||
		a.set_scissor_param != b.set_scissor_param ||
		a.prims.size() != b.prims.size())
		return false;
	if (a.modulate_set && a.modulate != b.modulate) return false;
	if (a.set_scissor_param && a.scissor_param != b.scissor_param) return false;
	for (size_t i = 0; i < a.prims.size(); ++i) {
		if (!_prim_equal(a.prims[i], b.prims[i])) return false;
	}
	return true;
}

// (Re)apply a slot's complete state to its canvas item. Always sets every
// property to a defined value (not just the ones that differ from defaults) so
// a recycled RID never carries stale group-mode/modulate/material from whatever
// it held last frame. Called only for new or changed slots.
void RmlContext::_apply_slot(int slot_index, const SlotDesc& desc,
		const godot::RID& parent_rid, const godot::RID& item) {
	auto* rs = godot::RenderingServer::get_singleton();
	if (rs == nullptr) return;

	rs->canvas_item_set_parent(item, parent_rid);
	rs->canvas_item_set_material(item, desc.material);
	rs->canvas_item_set_default_texture_filter(item,
		static_cast<godot::RenderingServer::CanvasItemTextureFilter>(desc.filter));
	if (desc.draw_index >= 0)
		rs->canvas_item_set_draw_index(item, desc.draw_index);
	rs->canvas_item_set_canvas_group_mode(item,
		desc.group_mode >= 0
			? static_cast<godot::RenderingServer::CanvasGroupMode>(desc.group_mode)
			: godot::RenderingServer::CANVAS_GROUP_MODE_DISABLED);
	rs->canvas_item_set_modulate(item, desc.modulate);
	if (desc.set_scissor_param)
		rs->canvas_item_set_instance_shader_parameter(item, "scissor_rect", desc.scissor_param);

	ClipResult clip_buf;
	_frame_stats.prims_applied += static_cast<uint32_t>(desc.prims.size());
	for (const auto& p : desc.prims) {
		if (p.kind == SlotPrim::MESH) {
			rs->canvas_item_add_mesh(item, p.mesh_rid, p.xform, p.modulate, p.tex_rid);
		} else {
			_frame_stats.tri_clips++;
			const auto* raw = _render_interface.get_raw_geometry(
				static_cast<Rml::CompiledGeometryHandle>(p.geo_handle));
			if (raw && _clip_mesh_to_rect(*raw, p.xform, p.clip_rect, clip_buf)) {
				rs->canvas_item_add_triangle_array(item,
					clip_buf.indices, clip_buf.positions, clip_buf.colors,
					clip_buf.uvs, godot::PackedInt32Array(),
					godot::PackedFloat32Array(), p.tex_rid);
			}
		}
	}
}

// Diff this frame's slot descriptors against the previous frame's slots by
// position. Identical slot -> reuse its canvas item untouched (zero RS calls);
// changed slot -> reuse the same RID via canvas_item_clear + re-apply; surplus
// previous slots -> freed. Slots are processed in creation order, so a parent
// is always finalized before its children read its (possibly new) RID.
void RmlContext::_reconcile_slots(size_t used) {
	auto* rs = godot::RenderingServer::get_singleton();
	if (rs == nullptr) return;

	const godot::RID root_canvas = get_canvas_item();

	std::vector<Slot>& prev = _slots_buf[_slots_cur];
	std::vector<Slot>& next = _slots_buf[1 - _slots_cur];

	for (size_t i = 0; i < used; ++i) {
		Slot& s = next[i];
		const godot::RID parent_rid = (s.desc.parent < 0) ? root_canvas : next[s.desc.parent].rid;
		s.parent_rid = parent_rid;

		const bool have_prev = (i < _slots_count) && prev[i].rid.is_valid();
		if (have_prev && prev[i].desc.kind == s.desc.kind) {
			s.rid = prev[i].rid;
			const bool same = prev[i].parent_rid == parent_rid &&
				_desc_equal(prev[i].desc, s.desc);
			if (!same) {
				_frame_stats.slots_reapplied++;
				rs->canvas_item_clear(s.rid);
				_apply_slot(static_cast<int>(i), s.desc, parent_rid, s.rid);
			} else {
				_frame_stats.slots_reused++;
			}
			// else: fully reuse — the canvas item already holds the right state.
		} else {
			if (have_prev) { rs->free_rid(prev[i].rid); _frame_stats.slots_freed++; } // kind changed at this slot
			_frame_stats.slots_created++;
			s.rid = rs->canvas_item_create();
			_apply_slot(static_cast<int>(i), s.desc, parent_rid, s.rid);
		}
	}

	// Free previous-frame slots that this frame no longer produces.
	for (size_t i = used; i < _slots_count; ++i) {
		if (prev[i].rid.is_valid()) { rs->free_rid(prev[i].rid); _frame_stats.slots_freed++; }
	}

	_slots_count = used;
	_slots_cur = static_cast<uint8_t>(1 - _slots_cur);
}

void RmlContext::_free_all_slots() {
	auto* rs = godot::RenderingServer::get_singleton();
	std::vector<Slot>& prev = _slots_buf[_slots_cur];
	if (rs != nullptr) {
		for (size_t i = 0; i < _slots_count && i < prev.size(); ++i) {
			if (prev[i].rid.is_valid()) {
				rs->free_rid(prev[i].rid);
				prev[i].rid = godot::RID();
			}
		}
	}
	_slots_count = 0;
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

godot::Dictionary RmlContext::get_frame_stats() const {
	godot::Dictionary d;
	d["draw_commands"] = static_cast<int>(_frame_stats.draw_commands);
	d["slots_used"] = static_cast<int>(_frame_stats.slots_used);
	d["slots_reused"] = static_cast<int>(_frame_stats.slots_reused);
	d["slots_reapplied"] = static_cast<int>(_frame_stats.slots_reapplied);
	d["slots_created"] = static_cast<int>(_frame_stats.slots_created);
	d["slots_freed"] = static_cast<int>(_frame_stats.slots_freed);
	d["prims_applied"] = static_cast<int>(_frame_stats.prims_applied);
	d["tri_clips"] = static_cast<int>(_frame_stats.tri_clips);
	d["render_us"] = static_cast<int64_t>(_frame_stats.render_us);
	d["build_us"] = static_cast<int64_t>(_frame_stats.build_us);
	d["reconcile_us"] = static_cast<int64_t>(_frame_stats.reconcile_us);
	d["update_us"] = static_cast<int64_t>(_last_update_us);
	d["embed_update_us"] = static_cast<int64_t>(_last_embed_update_us);
	d["embed_us_by_id"] = _last_embed_us_by_id;
	d["sync_us"] = static_cast<int64_t>(_last_sync_us);
	d["hover_us"] = static_cast<int64_t>(_last_hover_us);
	d["input_us"] = static_cast<int64_t>(_last_input_us);
	d["input_events"] = static_cast<int>(_last_input_events);
	return d;
}

// --- Private: Input forwarding ---

} // namespace RmlGodot
