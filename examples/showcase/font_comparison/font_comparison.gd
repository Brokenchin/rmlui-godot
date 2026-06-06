extends Control

## Font comparison: both sides use the SAME FontFile resource loaded via
## load_font_resource() so Godot's import settings (hinting, subpixel, etc.)
## are preserved on the RmlUI side — no overrides, true apples-to-apples.
##
## Known issues being diagnosed:
## - 8px "Tight" line: mystery gap after first 'm' in "mmmwww"
## - Symbols ≤≥≠ may not render on RmlUI side (missing glyph / fallback)

const FONT_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const FONT_BOLD_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf"

const PANGRAM := "The quick brown fox jumps over the lazy dog 0123456789"
const KERNING_PAIRS := ["Kerning pairs: AVATAR WAV Type To fly"]
const TIGHT := ["Tight: iiillllIIII mmmwww fiji ffl"]
const MIXED := [
	"Mixed: HP: 1250/1250 | MP: 340/500 | STR: 42 DEX: 38 INT: 55",
]
const SYMBOLS := ["Symbols: +–×÷ ≤≥≠ °©® €£¥ «»"]

# Gap isolation tests: problem pairs at START to rule out drift
const GAP_TESTS := [
	"mmmmmm mmmmmm mmmmmm",
	"wwwwww wwwwww wwwwww",
	"iiiiii iiiiii iiiiii",
	"llllll llllll llllll",
	"mmwwmmww mmwwmmww",
	"lazy lazy lazy lazy lazy",
	"brown brown brown brown",
	"all all all all all all",
	"av va av va av va av va",
]

var test_sizes := [8, 10, 12, 14, 16, 18, 20, 24, 32]
var font_res: FontFile
var font_bold_res: FontFile

func _ready() -> void:
	font_res = load(FONT_PATH) as FontFile
	font_bold_res = load(FONT_BOLD_PATH) as FontFile

	# --- Godot side (left) ---
	var godot_vbox: VBoxContainer = $Columns/GodotSide/GodotScroll/GodotVBox
	_add_header(godot_vbox, "Godot Label (native)")

	for sz in test_sizes:
		var text := "%dpx: %s" % [sz, PANGRAM]
		if sz >= 24:
			text = "%dpx: The quick brown fox jumps over the lazy dog" % sz
		if sz >= 32:
			text = "%dpx: The quick brown fox jumps" % sz
		_add_label(godot_vbox, text, sz)

	_add_separator(godot_vbox)
	for text in KERNING_PAIRS:
		_add_label(godot_vbox, text, 12)
		_add_label(godot_vbox, text, 16)
	for text in TIGHT:
		_add_label(godot_vbox, text, 8)
		_add_label(godot_vbox, text, 12)
		_add_label(godot_vbox, text, 16)

	_add_separator(godot_vbox)
	for text in MIXED:
		_add_label(godot_vbox, text, 10)
		_add_label(godot_vbox, text, 12)
	for text in SYMBOLS:
		_add_label(godot_vbox, text, 8)
		_add_label(godot_vbox, text, 12)

	_add_separator(godot_vbox)
	_add_header(godot_vbox, "Gap isolation (8px)")
	for text in GAP_TESTS:
		_add_label(godot_vbox, text, 8)

	# --- RmlUI side (right) ---
	var rml: RmlContext = $Columns/RmlSide/RmlContext
	rml.load_font_resource(font_res)
	rml.load_font_resource(font_bold_res)
	rml.load_document(
		"res://addons/rmlui-godot/examples/showcase/font_comparison/font_comparison.rml")

	# --- Direct-draw test (font_draw_glyph bypass) ---
	# Draws via Godot's native font_draw_glyph on a canvas item ON TOP of
	# the RmlUI rendering. If these look correct while the RmlUI text above
	# has gaps, the problem is in our atlas/mesh reconstruction.
	var y := 500.0
	var cyan := Color(0.2, 1.0, 0.8, 1.0)

	var header := Color(0.5, 0.5, 1.0)

	rml.add_direct_draw("--- DIRECT DRAW 8px ---", "Noto Sans", 10, Vector2(8, y), header)
	y += 14
	for text in GAP_TESTS:
		rml.add_direct_draw(text, "Noto Sans", 8, Vector2(8, y), cyan)
		y += 12
	for text in TIGHT:
		rml.add_direct_draw(text, "Noto Sans", 8, Vector2(8, y), cyan)
		y += 12
	for text in SYMBOLS:
		rml.add_direct_draw(text, "Noto Sans", 8, Vector2(8, y), cyan)
		y += 12

	y += 6
	rml.add_direct_draw("--- DIRECT DRAW 12px ---", "Noto Sans", 10, Vector2(8, y), header)
	y += 14
	rml.add_direct_draw(PANGRAM, "Noto Sans", 12, Vector2(8, y), cyan)
	y += 16
	for text in TIGHT:
		rml.add_direct_draw(text, "Noto Sans", 12, Vector2(8, y), cyan)
		y += 16
	for text in KERNING_PAIRS:
		rml.add_direct_draw(text, "Noto Sans", 12, Vector2(8, y), cyan)
		y += 16
	for text in MIXED:
		rml.add_direct_draw(text, "Noto Sans", 12, Vector2(8, y), cyan)
		y += 16
	for text in SYMBOLS:
		rml.add_direct_draw(text, "Noto Sans", 12, Vector2(8, y), cyan)
		y += 16

	y += 6
	rml.add_direct_draw("--- DIRECT DRAW 16px ---", "Noto Sans", 10, Vector2(8, y), header)
	y += 14
	rml.add_direct_draw(PANGRAM, "Noto Sans", 16, Vector2(8, y), cyan)
	y += 20
	for text in TIGHT:
		rml.add_direct_draw(text, "Noto Sans", 16, Vector2(8, y), cyan)
		y += 20
	for text in SYMBOLS:
		rml.add_direct_draw(text, "Noto Sans", 16, Vector2(8, y), cyan)
		y += 20

func _add_header(parent: Control, text: String) -> void:
	var lbl := Label.new()
	lbl.text = text
	lbl.add_theme_font_size_override("font_size", 14)
	lbl.add_theme_color_override("font_color", Color(0.4, 0.6, 1.0))
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	parent.add_child(lbl)

func _add_label(parent: Control, text: String, sz: int) -> void:
	var lbl := Label.new()
	lbl.text = text
	lbl.add_theme_font_override("font", font_res)
	lbl.add_theme_font_size_override("font_size", sz)
	lbl.add_theme_color_override("font_color", Color(0.88, 0.88, 0.88))
	parent.add_child(lbl)

func _add_separator(parent: Control) -> void:
	var sep := HSeparator.new()
	sep.add_theme_constant_override("separation", 8)
	parent.add_child(sep)
