shader_type canvas_item;
render_mode blend_premul_alpha;

// Auto-supplied by the bridge — used here to aspect-correct the ripple so it
// stays circular regardless of the element's width/height.
uniform vec2 element_dimensions;

uniform float speed = 2.0;
uniform float frequency = 24.0;
uniform vec4 wave_color : source_color = vec4(0.2, 0.8, 1.0, 1.0);
uniform vec4 bg_color : source_color = vec4(0.02, 0.05, 0.1, 1.0);

void fragment() {
	vec2 uv = UV - 0.5;
	uv.x *= element_dimensions.x / max(element_dimensions.y, 1.0);
	float d = length(uv);
	float wave = 0.5 + 0.5 * sin(d * frequency - TIME * speed);
	vec4 c = mix(bg_color, wave_color, wave);
	COLOR = vec4(c.rgb * c.a, c.a);
}
