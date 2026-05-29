shader_type canvas_item;
render_mode blend_premul_alpha;

// Auto-supplied by the bridge (element fill size in pixels).
uniform vec2 element_dimensions;

// Author uniforms — tweak from the inspector, or override per registration via
// register_decorator_material() with a configured ShaderMaterial.
uniform float speed = 1.0;
uniform float scale = 8.0;
uniform vec4 color_a : source_color = vec4(0.05, 0.0, 0.2, 1.0);
uniform vec4 color_b : source_color = vec4(1.0, 0.3, 0.6, 1.0);

void fragment() {
	vec2 p = UV * scale;
	float t = TIME * speed;
	float v = sin(p.x + t)
		+ sin(p.y + t)
		+ sin((p.x + p.y) * 0.5 + t)
		+ sin(length(p - scale * 0.5) - t);
	float m = 0.5 + 0.5 * sin(v * 0.785398);
	vec4 c = mix(color_a, color_b, m);
	COLOR = vec4(c.rgb * c.a, c.a);
}
