shader_type canvas_item;
render_mode blend_premul_alpha;

// Scissor rect in control-local pixels: (x, y, width, height).
// Set per canvas item via canvas_item_set_instance_shader_parameter().
// Default is effectively unbounded so unset items pass every fragment.
instance uniform vec4 scissor_rect = vec4(-1000000.0, -1000000.0, 2000000.0, 2000000.0);

varying vec2 v_local_pos;

void vertex() {
	// VERTEX already has the add_mesh transform applied, so this is in
	// control-local pixel space — the same space as scissor_rect.
	v_local_pos = VERTEX;
}

void fragment() {
	if (v_local_pos.x < scissor_rect.x ||
		v_local_pos.y < scissor_rect.y ||
		v_local_pos.x > scissor_rect.x + scissor_rect.z ||
		v_local_pos.y > scissor_rect.y + scissor_rect.w) {
		discard;
	}
	// Do NOT touch COLOR. Godot's default canvas pipeline already produced
	// the correct premultiplied result (including MSDF/font decode). Manually
	// sampling texture(TEXTURE, UV) here would bypass that and break text.
}
