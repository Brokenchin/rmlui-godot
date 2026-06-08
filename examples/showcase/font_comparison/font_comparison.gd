extends Control

## Side-by-side font comparison: Godot Label (left) vs RmlUI (right).
## Both sides use the same FontFile resource and matching colors so the
## only variable is the text rendering pipeline.

const FONT_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf"
const FONT_MATH_PATH := "res://addons/rmlui-godot/examples/fonts/NotoSansMath-Regular.ttf"
const TEXT_COLOR := Color(0xe0 / 255.0, 0xe0 / 255.0, 0xe0 / 255.0)
const HEADER_COLOR := Color(0x66 / 255.0, 0x88 / 255.0, 0xcc / 255.0)

const TESTS := {
	8: [
		"The quick brown fox jumps over the lazy dog",
		"Tight: iiillllIIII mmmwww fiji ffl",
		"Mixed: HP 1250/1250 | STR 42 DEX 38",
		"Symbols: +–×÷ ≤≥≠ °©® €£¥ «»",
		"all all all all all all all all",
	],
	10: [
		"The quick brown fox jumps over the lazy dog",
		"Tight: iiillllIIII mmmwww fiji ffl",
		"Mixed: HP 1250/1250 | STR 42 DEX 38",
		"Symbols: +–×÷ ≤≥≠ °©® €£¥ «»",
		"all all all all all all all all",
	],
	12: [
		"The quick brown fox jumps over the lazy dog",
		"Tight: iiillllIIII mmmwww fiji ffl",
		"Mixed: HP 1250/1250 | STR 42 DEX 38",
		"Symbols: +–×÷ ≤≥≠ °©® €£¥ «»",
		"Kerning: AVATAR WAV Type To fly",
	],
	14: [
		"The quick brown fox jumps over the lazy dog",
		"Tight: iiillllIIII mmmwww fiji ffl",
	],
	16: [
		"The quick brown fox jumps over the lazy dog",
		"Tight: iiillllIIII mmmwww fiji ffl",
	],
}

var font_res: FontFile
var font_math_res: FontFile

func _ready() -> void:
	font_res = load(FONT_PATH) as FontFile
	font_math_res = load(FONT_MATH_PATH) as FontFile
	font_res.fallbacks = [font_math_res]

	var godot_vbox: VBoxContainer = $Columns/GodotSide/GodotScroll/GodotVBox
	_add_title(godot_vbox, "Godot Label")

	for sz in TESTS:
		_add_size_header(godot_vbox, "%dpx" % sz)
		for text in TESTS[sz]:
			_add_label(godot_vbox, text, sz)
		_add_spacer(godot_vbox, 6)

	var rml: RmlContext = $Columns/RmlSide/RmlContext
	rml.load_font_resource(font_res)
	rml.load_font_resource_ex(font_math_res, "", 0, true)
	rml.load_document(
		"res://addons/rmlui-godot/examples/showcase/font_comparison/font_comparison.rml")

func _add_title(parent: Control, text: String) -> void:
	var lbl := Label.new()
	lbl.text = text
	lbl.add_theme_font_override("font", font_res)
	lbl.add_theme_font_size_override("font_size", 14)
	lbl.add_theme_color_override("font_color", Color(0x66 / 255.0, 0x99 / 255.0, 0xff / 255.0))
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	parent.add_child(lbl)

func _add_size_header(parent: Control, text: String) -> void:
	var lbl := Label.new()
	lbl.text = text
	lbl.add_theme_font_override("font", font_res)
	lbl.add_theme_font_size_override("font_size", 10)
	lbl.add_theme_color_override("font_color", HEADER_COLOR)
	parent.add_child(lbl)

func _add_label(parent: Control, text: String, sz: int) -> void:
	var lbl := Label.new()
	lbl.text = text
	lbl.add_theme_font_override("font", font_res)
	lbl.add_theme_font_size_override("font_size", sz)
	lbl.add_theme_color_override("font_color", TEXT_COLOR)
	parent.add_child(lbl)

func _add_spacer(parent: Control, height: int) -> void:
	var c := Control.new()
	c.custom_minimum_size.y = height
	parent.add_child(c)
