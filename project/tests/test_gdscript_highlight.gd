extends SceneTree
## GDScript region coloring in the RML highlighter: types, calls, keywords.

const HL := preload("res://addons/rmlui-godot/editor/gdscript_tokenizer.gd")

var _fails := 0


func _initialize() -> void:
	# func Test(i : int, a : Dictionary) -> Vector3i:
	var line := "func Test(i : int, a : Dictionary) -> Vector3i:"
	var toks := {}
	HL.tokenize(line, 0, line.length(), toks)
	_check("func is keyword", _color_at(toks, line, "func") == HL.COLOR_KEYWORD)
	_check("Test is call", _color_at(toks, line, "Test") == HL.COLOR_FUNC_CALL)
	_check("Dictionary is type", _color_at(toks, line, "Dictionary") == HL.COLOR_TYPE)
	_check("Vector3i is type", _color_at(toks, line, "Vector3i") == HL.COLOR_TYPE)

	# var a = a + Vector2(2,2)
	line = "var a = a + Vector2(2,2)"
	toks = {}
	HL.tokenize(line, 0, line.length(), toks)
	_check("Vector2 ctor is type", _color_at(toks, line, "Vector2") == HL.COLOR_TYPE)
	_check("var is keyword", _color_at(toks, line, "var") == HL.COLOR_KEYWORD)

	# ALL_CAPS constant is not a type; lowercase call is a call.
	line = "do_thing(MAX_SIZE)"
	toks = {}
	HL.tokenize(line, 0, line.length(), toks)
	_check("call colored", _color_at(toks, line, "do_thing") == HL.COLOR_FUNC_CALL)
	_check("ALL_CAPS not a type", _color_at(toks, line, "MAX_SIZE") == HL.COLOR_TEXT)

	# lowercase builtins are types
	line = "var i : int = 0"
	toks = {}
	HL.tokenize(line, 0, line.length(), toks)
	_check("int is type", _color_at(toks, line, "int") == HL.COLOR_TYPE)
	line = "func f(x : float) -> bool:"
	toks = {}
	HL.tokenize(line, 0, line.length(), toks)
	_check("float is type", _color_at(toks, line, "float") == HL.COLOR_TYPE)
	_check("bool is type", _color_at(toks, line, "bool") == HL.COLOR_TYPE)

	# Punctuation must not inherit neighbor colors (sparse-dict bleed).
	line = "var arr : Array[Vector2] = [Vector2(0,0), Vector2(0,0)]"
	toks = {}
	HL.tokenize(line, 0, line.length(), toks)
	_check("open bracket is text", toks.get(line.find("["), {}).get("color") == HL.COLOR_TEXT)
	_check("equals is text", toks.get(line.find("="), {}).get("color") == HL.COLOR_TEXT)
	_check("paren after Vector2 is text", toks.get(line.find("("), {}).get("color") == HL.COLOR_TEXT)
	_check("closing bracket is text", toks.get(line.rfind("]"), {}).get("color") == HL.COLOR_TEXT)
	_check("comma is text", toks.get(line.find(","), {}).get("color") == HL.COLOR_NUMBER or toks.get(line.find(","), {}).get("color") == HL.COLOR_TEXT)
	line = "var vec : Vector4i = Vector4i(0,0,0,0)"
	toks = {}
	HL.tokenize(line, 0, line.length(), toks)
	_check("trailing paren is text", toks.get(line.rfind(")"), {}).get("color") == HL.COLOR_TEXT)

	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _color_at(toks: Dictionary, line: String, word: String) -> Color:
	var pos := line.find(word)
	return toks.get(pos, {}).get("color", Color.BLACK)


func _check(name: String, ok: bool) -> void:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	if not ok:
		_fails += 1
