@tool
class_name RcssSyntaxHighlighter
extends EditorSyntaxHighlighter

# --- Colors (Catppuccin Mocha-inspired, matches Godot dark theme) ---
const COLOR_SELECTOR    := Color(0.537, 0.706, 0.980)  # blue — tag selectors
const COLOR_CLASS       := Color(0.647, 0.890, 0.631)  # green — .class
const COLOR_ID          := Color(0.976, 0.886, 0.686)  # yellow — #id
const COLOR_PSEUDO      := Color(0.804, 0.576, 0.969)  # mauve — :hover, :focus
const COLOR_PROPERTY    := Color(0.839, 0.757, 0.886)  # lavender — property names
const COLOR_VALUE       := Color(0.976, 0.702, 0.529)  # peach — values
const COLOR_NUMBER      := Color(0.976, 0.702, 0.529)  # peach — numbers & units
const COLOR_STRING      := Color(0.647, 0.890, 0.631)  # green — quoted strings
const COLOR_COLOR_LIT   := Color(0.949, 0.541, 0.659)  # pink — #hex colors
const COLOR_COMMENT     := Color(0.424, 0.443, 0.529)  # overlay1 — comments
const COLOR_BRACE       := Color(0.804, 0.839, 0.957)  # text — { } ; :
const COLOR_AT_RULE     := Color(0.949, 0.541, 0.659)  # pink — @media, @keyframes
const COLOR_IMPORTANT   := Color(0.949, 0.541, 0.659)  # pink — !important

const RMLUI_PROPERTIES := [
	"decorator", "font-effect", "drag", "tab-index",
	"nav-up", "nav-down", "nav-left", "nav-right",
	"scrollbar-margin", "overflow", "clip",
]

func _get_name() -> String:
	return "RCSS"

func _get_supported_languages() -> PackedStringArray:
	return PackedStringArray(["rcss"])

func _get_line_syntax_highlighting(line_num: int) -> Dictionary:
	var text := get_text_edit().get_line(line_num)
	var result := {}
	var i := 0
	var len := text.length()

	while i < len:
		var c := text[i]

		# Block comment /* ... */
		if c == "/" and i + 1 < len and text[i + 1] == "*":
			result[i] = {"color": COLOR_COMMENT}
			# Comment extends to end of line (multi-line handled by state)
			return result

		# Check if we're inside a comment continuation
		# (Godot doesn't give us cross-line state, so look for */)
		if c == "*" and i + 1 < len and text[i + 1] == "/":
			result[i] = {"color": COLOR_COMMENT}
			i += 2
			continue

		# @-rules
		if c == "@":
			var end := _scan_word(text, i + 1)
			result[i] = {"color": COLOR_AT_RULE}
			i = end
			continue

		# #id selector or #hex color
		if c == "#":
			var start := i
			i += 1
			var end := _scan_hex_or_word(text, i)
			if _is_in_value_context(text, start):
				result[start] = {"color": COLOR_COLOR_LIT}
			else:
				result[start] = {"color": COLOR_ID}
			i = end
			continue

		# .class selector
		if c == "." and not _is_in_value_context(text, i):
			var end := _scan_word(text, i + 1)
			result[i] = {"color": COLOR_CLASS}
			i = end
			continue

		# :pseudo-class or property separator
		if c == ":":
			if i + 1 < len and text[i + 1] != " " and text[i + 1] != ";" and not _is_in_value_context(text, i):
				var end := _scan_word(text, i + 1)
				result[i] = {"color": COLOR_PSEUDO}
				i = end
				continue
			else:
				result[i] = {"color": COLOR_BRACE}
				i += 1
				continue

		# Braces and semicolons
		if c in "{};,":
			result[i] = {"color": COLOR_BRACE}
			i += 1
			continue

		# Quoted strings
		if c == "\"" or c == "'":
			var end := _scan_string(text, i)
			result[i] = {"color": COLOR_STRING}
			i = end
			continue

		# Numbers (with units: dp, px, %, em, etc.)
		if c.is_valid_int() or (c == "-" and i + 1 < len and text[i + 1].is_valid_int()):
			var end := _scan_number(text, i)
			result[i] = {"color": COLOR_NUMBER}
			i = end
			continue

		# !important
		if c == "!":
			var rest := text.substr(i)
			if rest.begins_with("!important"):
				result[i] = {"color": COLOR_IMPORTANT}
				i += 10
				continue

		# Words (selectors or properties depending on context)
		if _is_word_char(c):
			var start := i
			var end := _scan_word(text, i)
			var word := text.substr(start, end - start)
			if _is_in_value_context(text, start):
				result[start] = {"color": COLOR_VALUE}
			elif _is_property_context(text, start):
				result[start] = {"color": COLOR_PROPERTY}
			else:
				result[start] = {"color": COLOR_SELECTOR}
			i = end
			continue

		i += 1

	return result


# --- Scanning helpers ---

func _scan_word(text: String, from: int) -> int:
	var i := from
	while i < text.length() and _is_word_char(text[i]):
		i += 1
	return i

func _scan_hex_or_word(text: String, from: int) -> int:
	var i := from
	while i < text.length() and _is_hex_or_word_char(text[i]):
		i += 1
	return i

func _scan_string(text: String, from: int) -> int:
	var quote := text[from]
	var i := from + 1
	while i < text.length():
		if text[i] == "\\" and i + 1 < text.length():
			i += 2
			continue
		if text[i] == quote:
			return i + 1
		i += 1
	return i

func _scan_number(text: String, from: int) -> int:
	var i := from
	if i < text.length() and text[i] == "-":
		i += 1
	while i < text.length() and (text[i].is_valid_int() or text[i] == "."):
		i += 1
	# Units: dp, px, em, %, rem, vh, vw, etc.
	if i < text.length() and text[i] == "%":
		return i + 1
	var unit_start := i
	while i < text.length() and text[i].to_lower() in "abcdefghijklmnopqrstuvwxyz":
		i += 1
	return i

func _is_word_char(c: String) -> bool:
	return c == "-" or c == "_" or c.to_lower() >= "a" and c.to_lower() <= "z" or c.is_valid_int()

func _is_hex_or_word_char(c: String) -> bool:
	return _is_word_char(c) or c.to_lower() in "abcdef"

func _is_in_value_context(text: String, pos: int) -> bool:
	# Check if we're after a ':' (inside a property value)
	var before := text.substr(0, pos).strip_edges(false, true)
	var colon_pos := before.rfind(":")
	var brace_pos := before.rfind("{")
	var semi_pos := before.rfind(";")
	if colon_pos == -1:
		return false
	return colon_pos > brace_pos and colon_pos > semi_pos

func _is_property_context(text: String, pos: int) -> bool:
	# Check if we're after '{' or ';' (start of a property declaration)
	var before := text.substr(0, pos).strip_edges(false, true)
	var brace_pos := before.rfind("{")
	var semi_pos := before.rfind(";")
	var colon_pos := before.rfind(":")
	if brace_pos == -1 and semi_pos == -1:
		return false
	var last_delimiter := maxi(brace_pos, semi_pos)
	return last_delimiter > colon_pos or colon_pos == -1
