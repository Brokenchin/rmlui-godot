#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <unordered_map>
#include <vector>

namespace RmlGodot {

class GodotRenderInterface final : public Rml::RenderInterface {
public:

	// --- Required: Geometry ---
	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

	// --- Required: Textures ---
	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture) override;

	// --- Required: Scissor ---
	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

	// --- Optional: Transform ---
	void SetTransform(const Rml::Matrix4f* transform) override;

	// --- Optional: Layers ---
	Rml::LayerHandle PushLayer() override;
	void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination,
		Rml::BlendMode blend_mode, Rml::Span<const Rml::CompiledFilterHandle> filters) override;
	void PopLayer() override;

	// --- Optional: Clip Mask ---
	void EnableClipMask(bool enable) override;
	void RenderToClipMask(Rml::ClipMaskOperation operation,
		Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override;

	// --- Optional: Filters ---
	Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override;
	void ReleaseFilter(Rml::CompiledFilterHandle filter) override;

	// --- Draw command types ---
	enum class CommandType : uint8_t {
		GEOMETRY,
		PUSH_LAYER,
		POP_LAYER,
		COMPOSITE_LAYERS,
		ENABLE_CLIP_MASK,
		RENDER_TO_CLIP_MASK,
	};

	struct DrawCommand {
		CommandType type = CommandType::GEOMETRY;

		// Geometry fields
		Rml::CompiledGeometryHandle geometry = 0;
		godot::Vector2 translation;
		Rml::TextureHandle texture = 0;
		bool scissor_enabled = false;
		godot::Rect2i scissor_rect;
		bool has_transform = false;
		godot::Transform2D transform;

		// Layer fields
		Rml::LayerHandle layer_handle = 0;
		Rml::LayerHandle source_layer = 0;
		Rml::LayerHandle dest_layer = 0;
		Rml::BlendMode blend_mode = Rml::BlendMode::Blend;
		std::vector<Rml::CompiledFilterHandle> filters;

		// Clip mask fields
		bool clip_mask_enabled = false;
		Rml::ClipMaskOperation clip_op{};
	};

	// --- Filter data ---
	struct FilterData {
		enum class Type : uint8_t { OPACITY, BLUR, DROP_SHADOW, BRIGHTNESS, CONTRAST, UNKNOWN };
		Type type = Type::UNKNOWN;
		float value = 1.0f;
	};

	const std::vector<DrawCommand>& get_draw_commands() const { return _draw_commands; }
	void clear_draw_commands() { _draw_commands.clear(); }
	void release_all_resources();

	godot::Ref<godot::ArrayMesh> get_mesh(Rml::CompiledGeometryHandle handle) const;
	godot::Ref<godot::Texture2D> get_texture_or_white(Rml::TextureHandle handle);
	const FilterData* get_filter(Rml::CompiledFilterHandle handle) const;

	size_t get_geometry_count() const { return _geometry.size(); }
	size_t get_texture_count() const { return _textures.size(); }
	size_t get_filter_count() const { return _filters.size(); }
	size_t get_draw_command_count() const { return _draw_commands.size(); }

	bool register_texture(const std::string& name, const godot::Ref<godot::Texture2D>& texture);
	bool unregister_texture(const std::string& name);
	const std::unordered_map<std::string, godot::Ref<godot::ImageTexture>>& get_registered_textures() const { return _registered_textures; }

private:
	void _ensure_white_texture();
	godot::Ref<godot::ImageTexture> _white_texture;

	std::unordered_map<uintptr_t, godot::Ref<godot::ArrayMesh>> _geometry;
	std::unordered_map<uintptr_t, godot::Ref<godot::ImageTexture>> _textures;
	std::unordered_map<uintptr_t, FilterData> _filters;
	std::vector<DrawCommand> _draw_commands;

	std::unordered_map<std::string, godot::Ref<godot::ImageTexture>> _registered_textures;

	uintptr_t _next_geo_handle = 1;
	uintptr_t _next_tex_handle = 1;
	uintptr_t _next_filter_handle = 1;
	Rml::LayerHandle _next_layer_handle = 1;

	bool _scissor_enabled = false;
	Rml::Rectanglei _scissor_region{};
	bool _has_transform = false;
	godot::Transform2D _current_transform;
	bool _clip_mask_enabled = false;
};

} // namespace RmlGodot
