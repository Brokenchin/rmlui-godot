shader_type canvas_item;
render_mode blend_premul_alpha;

// Template for custom RmlUi decorator shaders.
//
// Use from RCSS:   decorator: shader("my_shader");
// Register in GDScript:
//     $RmlContext.register_decorator_shader(
//         "my_shader", preload("res://path/to/this.gdshader"))
//
// Rules:
//  - render_mode MUST be blend_premul_alpha (RmlUi outputs premultiplied alpha).
//  - Output COLOR in premultiplied form: COLOR = vec4(rgb * a, a).
//  - Decorator geometry is a quad over the element; UV is normalized [0,1] over
//    the element fill area.
//  - TEXTURE is always bound: when the decorator has no image the bridge binds a
//    1x1 white texture, so texture(TEXTURE, UV) returns white. Sampling it is
//    safe; drive procedural effects from UV (and from a custom uniform sampler
//    if you need a real image).

// Supplied automatically by the bridge: element fill size in pixels.
uniform vec2 element_dimensions;

// Author-defined uniforms (set via the ShaderMaterial, shared per shader).
uniform vec4 color_a : source_color = vec4(0.2, 0.6, 1.0, 1.0);
uniform vec4 color_b : source_color = vec4(0.8, 0.2, 1.0, 1.0);

void fragment() {
	// Example: vertical gradient between the two colors.
	vec4 c = mix(color_a, color_b, UV.y);

	// Premultiplied-alpha output.
	COLOR = vec4(c.rgb * c.a, c.a);
}
