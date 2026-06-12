@tool
class_name RmlGdscriptTokenizer
extends RefCounted
## Minimal GDScript coloring for <script> block segments inside .rml files —
## keywords, strings, comments, numbers, CamelCase types and function calls.
## Not full fidelity; just enough to read. Separate from the highlighter so it
## loads (and tests) outside the editor, like RcssTokenizer.

const COLOR_TEXT      := Color(0.804, 0.839, 0.957)  # text
const COLOR_STRING    := Color(0.647, 0.890, 0.631)  # green
const COLOR_COMMENT   := Color(0.424, 0.443, 0.529)  # overlay1
const COLOR_KEYWORD   := Color(0.804, 0.576, 0.969)  # mauve
const COLOR_NUMBER    := Color(0.976, 0.702, 0.529)  # peach
const COLOR_TYPE      := Color(0.541, 0.886, 0.886)  # teal — Vector2, Node, MyClass
const COLOR_FUNC_CALL := Color(0.537, 0.706, 0.980)  # blue — calls

const KEYWORDS := [
	"func", "var", "const", "enum", "class", "class_name", "extends", "signal",
	"if", "elif", "else", "for", "while", "match", "when", "break", "continue",
	"pass", "return", "await", "yield", "static", "and", "or", "not", "in", "is",
	"as", "self", "super", "true", "false", "null", "void", "breakpoint", "tool",
]

# Lowercase builtins the CamelCase rule can't catch.
const BUILTIN_TYPES := ["int", "float", "bool", "str"]


static func tokenize(text: String, from: int, to: int, tokens: Dictionary) -> void:
	var i := from
	var prev_word := ""
	while i < to:
		var c := text[i]
		if c == "#":
			tokens[i] = {"color": COLOR_COMMENT}
			i = to
		elif c == "\"" or c == "'":
			tokens[i] = {"color": COLOR_STRING}
			i = _scan_string(text, i, to)
		elif c >= "0" and c <= "9":
			tokens[i] = {"color": COLOR_NUMBER}
			while i < to and (text[i].is_valid_int() or text[i] == "." or text[i] == "_" or text[i] == "x"):
				i += 1
		elif _is_name_char(c):
			var e := i
			while e < to and (_is_name_char(text[e]) or (text[e] >= "0" and text[e] <= "9")):
				e += 1
			var word := text.substr(i, e - i)
			var color := COLOR_TEXT
			if word in KEYWORDS:
				color = COLOR_KEYWORD
			elif word in BUILTIN_TYPES:
				color = COLOR_TYPE
			elif prev_word == "func" or prev_word == "signal":
				# Declared name — function color even when CamelCase.
				color = COLOR_FUNC_CALL
			elif is_type_name(word):
				# GDScript convention: types are CamelCase. Covers annotations
				# (x: Vector2, -> Vector3i) and constructor calls (Vector2(2,2)).
				color = COLOR_TYPE
			else:
				# foo( — color calls like the script editor does.
				var j := e
				while j < to and (text[j] == " " or text[j] == "\t"):
					j += 1
				if j < to and text[j] == "(":
					color = COLOR_FUNC_CALL
			tokens[i] = {"color": color}
			prev_word = word
			i = e
		else:
			# Punctuation/operators MUST emit a color: the highlighter dict is
			# sparse and a color runs until the next emitted column, so silent
			# chars inherit the previous token's color — 'Vector2(0,0)' bled
			# teal across the parens, 'Array[…] = […]' tinted the whole tail.
			if c != " " and c != "\t":
				tokens[i] = {"color": COLOR_TEXT}
			i += 1


## Starts uppercase and isn't an ALL_CAPS constant.
static func is_type_name(word: String) -> bool:
	if word.is_empty():
		return false
	var first := word[0]
	if first < "A" or first > "Z":
		return false
	if word.length() > 1 and word == word.to_upper() and word != word.to_lower():
		return false
	return true


static func _scan_string(text: String, from: int, to: int) -> int:
	var quote := text[from]
	var i := from + 1
	while i < to:
		if text[i] == "\\" and i + 1 < to:
			i += 2
			continue
		if text[i] == quote:
			return i + 1
		i += 1
	return to


static func _is_name_char(c: String) -> bool:
	# GDScript identifiers: letters and underscore (digits joined in the scan
	# loop). NOT ':' or '-' — unlike XML tag names ('x:int' is three tokens).
	var l := c.to_lower()
	return (l >= "a" and l <= "z") or c == "_"
