#include "GodotRenderInterface.hpp"
#include "RmlManager.hpp"

#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Variant.h>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace RmlGodot {

// --- Geometry ---

Rml::CompiledGeometryHandle GodotRenderInterface::CompileGeometry(
	Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {

	godot::PackedVector2Array positions;
	godot::PackedColorArray colors;
	godot::PackedVector2Array uvs;
	godot::PackedInt32Array idx;

	positions.resize(static_cast<int64_t>(vertices.size()));
	colors.resize(static_cast<int64_t>(vertices.size()));
	uvs.resize(static_cast<int64_t>(vertices.size()));
	idx.resize(static_cast<int64_t>(indices.size()));

	for (size_t i = 0; i < vertices.size(); ++i) {
		const auto& v = vertices[i];
		positions.set(static_cast<int64_t>(i),
			godot::Vector2(v.position.x, v.position.y));

		// RmlUI outputs premultiplied alpha — keep as-is, we use blend_premul_alpha.
		colors.set(static_cast<int64_t>(i), godot::Color(
			v.colour.red / 255.0f,
			v.colour.green / 255.0f,
			v.colour.blue / 255.0f,
			v.colour.alpha / 255.0f));

		uvs.set(static_cast<int64_t>(i),
			godot::Vector2(v.tex_coord.x, v.tex_coord.y));
	}

	for (size_t i = 0; i < indices.size(); ++i) {
		idx.set(static_cast<int64_t>(i), indices[i]);
	}

	godot::Array arrays;
	arrays.resize(godot::ArrayMesh::ARRAY_MAX);
	arrays[godot::ArrayMesh::ARRAY_VERTEX] = positions;
	arrays[godot::ArrayMesh::ARRAY_COLOR] = colors;
	arrays[godot::ArrayMesh::ARRAY_TEX_UV] = uvs;
	arrays[godot::ArrayMesh::ARRAY_INDEX] = idx;

	godot::Ref<godot::ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);

	uintptr_t handle = _next_geo_handle++;
	_geometry[handle] = mesh;
	_raw_geometry[handle] = {positions, colors, uvs, idx};
	return handle;
}

void GodotRenderInterface::RenderGeometry(
	const Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {

	DrawCommand cmd;
	cmd.geometry = geometry;
	cmd.translation = godot::Vector2(translation.x, translation.y);
	cmd.texture = texture;
	cmd.scissor_enabled = _scissor_enabled;
	cmd.scissor_rect = godot::Rect2i(
		_scissor_region.Left(), _scissor_region.Top(),
		_scissor_region.Width(), _scissor_region.Height());
	cmd.has_transform = _has_transform;
	cmd.transform = _current_transform;

	_draw_commands.push_back(cmd);
}

void GodotRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
	_geometry.erase(geometry);
	_raw_geometry.erase(geometry);
}

// --- Textures ---

Rml::TextureHandle GodotRenderInterface::LoadTexture(
	Rml::Vector2i& texture_dimensions, const Rml::String& source) {

	auto reg_it = _registered_textures.find(source);
	if (reg_it != _registered_textures.end() && reg_it->second.is_valid()) {
		uintptr_t handle = _next_tex_handle++;
		_textures[handle] = reg_it->second;
		texture_dimensions.x = static_cast<int>(reg_it->second->get_width());
		texture_dimensions.y = static_cast<int>(reg_it->second->get_height());
		return handle;
	}

	auto* manager = RmlManager::get_singleton();
	if (manager) {
		godot::String source_name(source.c_str());
		godot::Ref<godot::Texture2D> global_tex = manager->get_texture(source_name);
		if (global_tex.is_valid()) {
			godot::Ref<godot::Image> img = global_tex->get_image();
			if (img.is_valid()) {
				if (img->is_compressed()) img->decompress();
				img->premultiply_alpha();
				godot::Ref<godot::ImageTexture> img_tex = godot::ImageTexture::create_from_image(img);
				if (img_tex.is_valid()) {
					_registered_textures[source] = img_tex;
					uintptr_t handle = _next_tex_handle++;
					_textures[handle] = img_tex;
					texture_dimensions.x = static_cast<int>(img_tex->get_width());
					texture_dimensions.y = static_cast<int>(img_tex->get_height());
					return handle;
				}
			}
		}
	}

	godot::String gd_path = godot::String(source.c_str());
	if (!gd_path.begins_with("res://") && !gd_path.begins_with("user://") && !gd_path.begins_with("/")) {
		gd_path = godot::String("res://") + gd_path;
	}

	godot::Ref<godot::Texture2D> tex = godot::ResourceLoader::get_singleton()->load(gd_path);
	if (!tex.is_valid()) {
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] LoadTexture failed: ") + gd_path);
		return 0;
	}

	godot::Ref<godot::Image> img = tex->get_image();
	if (!img.is_valid()) return 0;
	if (img->is_compressed()) img->decompress();
	img->premultiply_alpha();

	godot::Ref<godot::ImageTexture> img_tex = godot::ImageTexture::create_from_image(img);
	if (!img_tex.is_valid()) return 0;

	texture_dimensions.x = static_cast<int>(img_tex->get_width());
	texture_dimensions.y = static_cast<int>(img_tex->get_height());

	uintptr_t handle = _next_tex_handle++;
	_textures[handle] = img_tex;
	return handle;
}

bool GodotRenderInterface::register_texture(const std::string& name, const godot::Ref<godot::Texture2D>& texture) {
	if (!texture.is_valid()) return false;

	godot::Ref<godot::Image> img = texture->get_image();
	if (!img.is_valid()) return false;

	if (img->is_compressed())
		img->decompress();
	img->premultiply_alpha();

	godot::Ref<godot::ImageTexture> img_tex = godot::ImageTexture::create_from_image(img);
	if (!img_tex.is_valid()) return false;

	_registered_textures[name] = img_tex;
	return true;
}

bool GodotRenderInterface::unregister_texture(const std::string& name) {
	return _registered_textures.erase(name) > 0;
}

Rml::TextureHandle GodotRenderInterface::GenerateTexture(
	Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {

	godot::PackedByteArray data;
	data.resize(static_cast<int64_t>(source.size()));
	memcpy(data.ptrw(), source.data(), source.size());

	godot::Ref<godot::Image> img = godot::Image::create_from_data(
		source_dimensions.x, source_dimensions.y, false, godot::Image::FORMAT_RGBA8, data);

	if (!img.is_valid()) return 0;
	img->premultiply_alpha();

	godot::Ref<godot::ImageTexture> tex = godot::ImageTexture::create_from_image(img);
	if (!tex.is_valid()) return 0;

	uintptr_t handle = _next_tex_handle++;
	_textures[handle] = tex;
	return handle;
}

void GodotRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
	_textures.erase(texture);
}

// --- Scissor ---

void GodotRenderInterface::EnableScissorRegion(bool enable) {
	_scissor_enabled = enable;
}

void GodotRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
	_scissor_region = region;
}

// --- Transform ---

void GodotRenderInterface::SetTransform(const Rml::Matrix4f* transform) {
	if (transform != nullptr) {
		_has_transform = true;
		// Extract 2D affine from the 4x4 column-major matrix.
		const auto* m = transform->data();
		_current_transform = godot::Transform2D(
			godot::Vector2(m[0], m[1]),   // column 0 (x-axis)
			godot::Vector2(m[4], m[5]),   // column 1 (y-axis)
			godot::Vector2(m[12], m[13])  // column 3 (origin)
		);
	} else {
		_has_transform = false;
		_current_transform = godot::Transform2D();
	}
}

// --- Layers ---

Rml::LayerHandle GodotRenderInterface::PushLayer() {
	Rml::LayerHandle handle = _next_layer_handle++;

	DrawCommand cmd;
	cmd.type = CommandType::PUSH_LAYER;
	cmd.layer_handle = handle;
	_draw_commands.push_back(cmd);

	return handle;
}

void GodotRenderInterface::CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination,
	Rml::BlendMode blend_mode, Rml::Span<const Rml::CompiledFilterHandle> filters) {

	DrawCommand cmd;
	cmd.type = CommandType::COMPOSITE_LAYERS;
	cmd.source_layer = source;
	cmd.dest_layer = destination;
	cmd.blend_mode = blend_mode;
	cmd.filters.assign(filters.begin(), filters.end());
	_draw_commands.push_back(cmd);
}

void GodotRenderInterface::PopLayer() {
	DrawCommand cmd;
	cmd.type = CommandType::POP_LAYER;
	_draw_commands.push_back(cmd);
}

// --- Clip Mask ---

void GodotRenderInterface::EnableClipMask(bool enable) {
	_clip_mask_enabled = enable;

	DrawCommand cmd;
	cmd.type = CommandType::ENABLE_CLIP_MASK;
	cmd.clip_mask_enabled = enable;
	_draw_commands.push_back(cmd);
}

void GodotRenderInterface::RenderToClipMask(Rml::ClipMaskOperation operation,
	Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) {

	DrawCommand cmd;
	cmd.type = CommandType::RENDER_TO_CLIP_MASK;
	cmd.clip_op = operation;
	cmd.geometry = geometry;
	cmd.translation = godot::Vector2(translation.x, translation.y);
	cmd.scissor_enabled = _scissor_enabled;
	cmd.scissor_rect = godot::Rect2i(
		_scissor_region.Left(), _scissor_region.Top(),
		_scissor_region.Width(), _scissor_region.Height());
	cmd.has_transform = _has_transform;
	cmd.transform = _current_transform;
	_draw_commands.push_back(cmd);
}

// --- Filters ---

Rml::CompiledFilterHandle GodotRenderInterface::CompileFilter(
	const Rml::String& name, const Rml::Dictionary& parameters) {

	FilterData filter;

	if (name == "opacity") {
		filter.type = FilterData::Type::OPACITY;
		auto it = parameters.find("value");
		if (it != parameters.end())
			filter.value = it->second.Get<float>(1.0f);
	} else if (name == "blur") {
		filter.type = FilterData::Type::BLUR;
		auto it = parameters.find("radius");
		if (it != parameters.end())
			filter.value = it->second.Get<float>(0.0f);
	} else if (name == "brightness") {
		filter.type = FilterData::Type::BRIGHTNESS;
		auto it = parameters.find("value");
		if (it != parameters.end())
			filter.value = it->second.Get<float>(1.0f);
	} else if (name == "contrast") {
		filter.type = FilterData::Type::CONTRAST;
		auto it = parameters.find("value");
		if (it != parameters.end())
			filter.value = it->second.Get<float>(1.0f);
	} else if (name == "drop-shadow") {
		filter.type = FilterData::Type::DROP_SHADOW;
	} else {
		filter.type = FilterData::Type::UNKNOWN;
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] Unsupported filter: ") + godot::String(name.c_str()));
	}

	uintptr_t handle = _next_filter_handle++;
	_filters[handle] = filter;
	return handle;
}

void GodotRenderInterface::ReleaseFilter(Rml::CompiledFilterHandle filter) {
	_filters.erase(filter);
}

// --- Shaders (decorator: shader(...)) ---

bool GodotRenderInterface::register_shader(const std::string& name, const godot::Ref<godot::Shader>& shader) {
	if (!shader.is_valid()) return false;
	_registered_shaders[name] = shader;
	return true;
}

bool GodotRenderInterface::unregister_shader(const std::string& name) {
	return _registered_shaders.erase(name) > 0;
}

Rml::CompiledShaderHandle GodotRenderInterface::CompileShader(
	const Rml::String& /*name*/, const Rml::Dictionary& parameters) {

	// RmlUi's built-in shader decorator always passes name == "shader" and the
	// user-facing name from `decorator: shader("<value>")` under "value".
	auto value_it = parameters.find("value");
	if (value_it == parameters.end()) return 0;
	std::string shader_name(value_it->second.Get<Rml::String>().c_str());

	auto reg_it = _registered_shaders.find(shader_name);
	if (reg_it == _registered_shaders.end() || !reg_it->second.is_valid()) {
		// No Godot shader registered for this name — returning 0 makes RmlUi fall
		// back to ordinary geometry rendering for this decorator.
		godot::UtilityFunctions::push_warning(
			godot::String("[RmlUi] No decorator shader registered for: ") +
			godot::String(shader_name.c_str()));
		return 0;
	}

	ShaderData data;
	data.name = shader_name;
	data.material.instantiate();
	data.material->set_shader(reg_it->second);

	auto dim_it = parameters.find("dimensions");
	if (dim_it != parameters.end()) {
		auto dims = dim_it->second.Get<Rml::Vector2f>();
		data.dimensions = godot::Vector2(dims.x, dims.y);
		data.material->set_shader_parameter("element_dimensions", data.dimensions);
	}

	uintptr_t handle = _next_shader_handle++;
	_shaders[handle] = std::move(data);
	return handle;
}

void GodotRenderInterface::RenderShader(Rml::CompiledShaderHandle shader,
	Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) {

	DrawCommand cmd;
	cmd.type = CommandType::SHADER_GEOMETRY;
	cmd.geometry = geometry;
	cmd.shader_handle = shader;
	cmd.translation = godot::Vector2(translation.x, translation.y);
	cmd.texture = texture;
	cmd.scissor_enabled = _scissor_enabled;
	cmd.scissor_rect = godot::Rect2i(
		_scissor_region.Left(), _scissor_region.Top(),
		_scissor_region.Width(), _scissor_region.Height());
	cmd.has_transform = _has_transform;
	cmd.transform = _current_transform;
	_draw_commands.push_back(cmd);
}

void GodotRenderInterface::ReleaseShader(Rml::CompiledShaderHandle shader) {
	_shaders.erase(shader);
}

// --- Lifecycle ---

void GodotRenderInterface::release_all_resources() {
	_geometry.clear();
	_raw_geometry.clear();
	_textures.clear();
	_filters.clear();
	_shaders.clear();
	_draw_commands.clear();
	_registered_textures.clear();
	_registered_shaders.clear();
	_white_texture.unref();
	_next_geo_handle = 1;
	_next_tex_handle = 1;
	_next_filter_handle = 1;
	_next_shader_handle = 1;
	_next_layer_handle = 1;
	_scissor_enabled = false;
	_scissor_region = {};
	_has_transform = false;
	_current_transform = godot::Transform2D();
	_clip_mask_enabled = false;
}

// --- Accessors ---

godot::Ref<godot::ArrayMesh> GodotRenderInterface::get_mesh(Rml::CompiledGeometryHandle handle) const {
	auto it = _geometry.find(handle);
	return it != _geometry.end() ? it->second : godot::Ref<godot::ArrayMesh>();
}

const GodotRenderInterface::RawGeometry* GodotRenderInterface::get_raw_geometry(
		Rml::CompiledGeometryHandle handle) const {
	auto it = _raw_geometry.find(handle);
	return it != _raw_geometry.end() ? &it->second : nullptr;
}

void GodotRenderInterface::_ensure_white_texture() {
	if (_white_texture.is_valid()) return;

	godot::PackedByteArray px;
	px.resize(4);
	px.set(0, 255);
	px.set(1, 255);
	px.set(2, 255);
	px.set(3, 255);

	godot::Ref<godot::Image> img = godot::Image::create_from_data(1, 1, false, godot::Image::FORMAT_RGBA8, px);
	_white_texture = godot::ImageTexture::create_from_image(img);
}

godot::Ref<godot::Texture2D> GodotRenderInterface::get_texture_or_white(Rml::TextureHandle handle) {
	if (handle != 0) {
		auto it = _textures.find(handle);
		if (it != _textures.end()) return it->second;
	}
	_ensure_white_texture();
	return _white_texture;
}

const GodotRenderInterface::FilterData* GodotRenderInterface::get_filter(
	Rml::CompiledFilterHandle handle) const {
	auto it = _filters.find(handle);
	return it != _filters.end() ? &it->second : nullptr;
}

const GodotRenderInterface::ShaderData* GodotRenderInterface::get_shader(
	Rml::CompiledShaderHandle handle) const {
	auto it = _shaders.find(handle);
	return it != _shaders.end() ? &it->second : nullptr;
}

} // namespace RmlGodot
