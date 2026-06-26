shader_type canvas_item;
render_mode blend_premul_alpha;

// Built-in RmlUi gradient decorators: linear-gradient, radial-gradient,
// conic-gradient and their repeating-* variants. RmlUi requests these through the
// same render-interface CompileShader entrypoint as custom shader() decorators,
// but with the gradient type as the name and gradient parameters (color stops,
// geometry) instead of a "value" string. GodotRenderInterface::CompileShader
// builds one ShaderMaterial per decorated element from this shader and pushes the
// per-element uniforms below.
//
// This mirrors RmlUi's reference GL3 backend gradient shader so the result
// matches the reference renderer (RmlUi_Renderer_GL3.cpp, shader_frag_gradient).
//
// UV carries the fragment position relative to the element fill offset, in pixels
// (RmlUi sets tex_coord = vertex.position - fill_offset for gradient geometry),
// which is the same space the gradient geometry (p0/p1, center, radius) lives in.

const int MAX_NUM_STOPS = 16; // Must match GodotRenderInterface (and GL3 backend).
// Note: PI is a built-in constant in Godot's shading language — do not redeclare.

// Gradient function selector. Must match GradientFunction in GodotRenderInterface:
//   0 LINEAR  1 RADIAL  2 CONIC  3 REPEATING_LINEAR  4 REPEATING_RADIAL  5 REPEATING_CONIC
uniform int func;
uniform vec2 p; // linear: starting point;          radial/conic: center
uniform vec2 v; // linear: vector to ending point;  radial: inverse radius;  conic: angled unit vector
uniform vec4 stop_colors[MAX_NUM_STOPS];     // premultiplied alpha, straight from RmlUi
uniform float stop_positions[MAX_NUM_STOPS]; // normalized, 0 -> start, 1 -> end
uniform int num_stops;

vec4 mix_stop_colors(float t) {
	vec4 color = stop_colors[0];
	for (int i = 1; i < num_stops; i++) {
		color = mix(color, stop_colors[i], smoothstep(stop_positions[i - 1], stop_positions[i], t));
	}
	return color;
}

void fragment() {
	float t = 0.0;
	vec2 ftc = UV;

	if (func == 0 || func == 3) {        // linear / repeating-linear
		float dist_square = dot(v, v);
		vec2 V = ftc - p;
		t = dot(v, V) / dist_square;
	} else if (func == 1 || func == 4) { // radial / repeating-radial
		vec2 V = ftc - p;
		t = length(v * V);
	} else if (func == 2 || func == 5) { // conic / repeating-conic
		mat2 R = mat2(vec2(v.x, -v.y), vec2(v.y, v.x));
		vec2 V = R * (ftc - p);
		t = 0.5 + atan(-V.x, V.y) / (2.0 * PI);
	}

	if (func == 3 || func == 4 || func == 5) { // repeating variants
		float t0 = stop_positions[0];
		float t1 = stop_positions[num_stops - 1];
		t = t0 + mod(t - t0, t1 - t0);
	}

	// Incoming COLOR carries the element's premultiplied opacity (RmlUi outputs
	// premultiplied alpha); TEXTURE is the bridge's 1x1 white texture so it does
	// not affect the result. Multiply by the mixed stop color (also premultiplied).
	COLOR *= mix_stop_colors(t);
}
