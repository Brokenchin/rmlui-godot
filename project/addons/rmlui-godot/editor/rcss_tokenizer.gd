@tool
class_name RcssTokenizer
extends RefCounted
## Line-based RCSS tokenizer with cross-line region state.
##
## Used by both RcssSyntaxHighlighter (whole .rcss files) and
## RmlSyntaxHighlighter (embedded <style> blocks and style="" attributes).
## tokenize() processes one line segment given the region state at its start
## and returns the syntax-highlighting tokens plus the state at its end.

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

# --- Region state encoding (carried across lines, must fit in a few bits) ---
const DEPTH_MASK   := 0x0F  # brace nesting depth, 0 = selector context
const FLAG_COMMENT := 0x10  # inside /* ... */
const FLAG_VALUE   := 0x20  # inside a property value (after ':')
const STATE_BITS   := 6     # total bits used — embedders shift around this

## State representing the inside of a declaration block — entry state for
## style="" attribute content, where there are no selectors.
const STATE_DECLARATIONS := 1


## Tokenize text[from..to) given the region state at `from`.
## Returns { "tokens": {column -> {"color": Color}}, "state": int }.
func tokenize(text: String, from: int, to: int, entry_state: int) -> Dictionary:
	to = mini(to, text.length())
	var tokens := {}
	var depth := entry_state & DEPTH_MASK
	var in_comment := (entry_state & FLAG_COMMENT) != 0
	var in_value := (entry_state & FLAG_VALUE) != 0
	var i := from

	while i < to:
		var c := text[i]

		# --- Comment continuation ---
		if in_comment:
			tokens[i] = {"color": COLOR_COMMENT}
			var close := text.find("*/", i)
			if close == -1 or close + 2 > to:
				i = to
			else:
				i = close + 2
				in_comment = false
			continue

		# --- Comment start ---
		if c == "/" and i + 1 < to and text[i + 1] == "*":
			in_comment = true
			continue  # the comment branch above emits the token

		# --- Quoted strings ---
		if c == "\"" or c == "'":
			tokens[i] = {"color": COLOR_STRING}
			i = _scan_string(text, i, to)
			continue

		if depth == 0:
			# --- Selector context ---
			if c == "@":
				tokens[i] = {"color": COLOR_AT_RULE}
				i = _scan_word(text, i + 1, to)
			elif c == ".":
				tokens[i] = {"color": COLOR_CLASS}
				i = _scan_word(text, i + 1, to)
			elif c == "#":
				tokens[i] = {"color": COLOR_ID}
				i = _scan_hex_or_word(text, i + 1, to)
			elif c == ":":
				tokens[i] = {"color": COLOR_PSEUDO}
				i = _scan_word(text, i + 1, to)
			elif c == "{":
				tokens[i] = {"color": COLOR_BRACE}
				depth = 1
				in_value = false
				i += 1
			elif c == "," or c == ";":
				tokens[i] = {"color": COLOR_BRACE}
				i += 1
			elif _is_word_char(c):
				tokens[i] = {"color": COLOR_SELECTOR}
				i = _scan_word(text, i, to)
			else:
				i += 1
			continue

		# --- Inside a block (depth > 0) ---
		if c == "}":
			tokens[i] = {"color": COLOR_BRACE}
			depth = maxi(depth - 1, 0)
			in_value = false
			i += 1
			continue
		if c == "{":
			tokens[i] = {"color": COLOR_BRACE}
			depth = mini(depth + 1, DEPTH_MASK)
			in_value = false
			i += 1
			continue
		if c == ";":
			tokens[i] = {"color": COLOR_BRACE}
			in_value = false
			i += 1
			continue

		if in_value:
			# --- Property value ---
			if c == "#":
				tokens[i] = {"color": COLOR_COLOR_LIT}
				i = _scan_hex_or_word(text, i + 1, to)
			elif c == "!" and text.substr(i, 10) == "!important":
				tokens[i] = {"color": COLOR_IMPORTANT}
				i += 10
			elif _is_digit(c) or (c == "-" and i + 1 < to and _is_digit(text[i + 1])):
				tokens[i] = {"color": COLOR_NUMBER}
				i = _scan_number(text, i, to)
			elif _is_word_char(c):
				tokens[i] = {"color": COLOR_VALUE}
				i = _scan_word(text, i, to)
			else:
				i += 1
			continue

		# --- Block, before ':' — property name or nested selector (@media) ---
		if _is_word_char(c):
			var word_end := _scan_word(text, i, to)
			if _declaration_ahead(text, word_end, to):
				tokens[i] = {"color": COLOR_PROPERTY}
				i = word_end
				while i < to and (text[i] == " " or text[i] == "\t"):
					i += 1
				if i < to and text[i] == ":":
					tokens[i] = {"color": COLOR_BRACE}
					in_value = true
					i += 1
			else:
				tokens[i] = {"color": COLOR_SELECTOR}
				i = word_end
			continue
		if c == ".":
			tokens[i] = {"color": COLOR_CLASS}
			i = _scan_word(text, i + 1, to)
			continue
		if c == "#":
			tokens[i] = {"color": COLOR_ID}
			i = _scan_hex_or_word(text, i + 1, to)
			continue
		if c == ":":
			tokens[i] = {"color": COLOR_PSEUDO}
			i = _scan_word(text, i + 1, to)
			continue
		if c == ",":
			tokens[i] = {"color": COLOR_BRACE}
			i += 1
			continue
		i += 1

	var state := depth
	if in_comment:
		state |= FLAG_COMMENT
	if in_value:
		state |= FLAG_VALUE
	return {"tokens": tokens, "state": state}


## True when the text after a word reads like "  : <value> ;" rather than a
## nested selector like "div:hover {" — i.e. no '{' before the ';' / segment end.
func _declaration_ahead(text: String, pos: int, to: int) -> bool:
	var i := pos
	while i < to and (text[i] == " " or text[i] == "\t"):
		i += 1
	if i >= to or text[i] != ":":
		return false
	i += 1
	while i < to:
		var c := text[i]
		if c == "{":
			return false
		if c == ";" or c == "}":
			return true
		i += 1
	return true  # declaration continues past end of line


# --- Scanning helpers ---

func _scan_word(text: String, from: int, to: int) -> int:
	var i := from
	while i < to and _is_word_char(text[i]):
		i += 1
	return maxi(i, from + 1) if from < to else to

func _scan_hex_or_word(text: String, from: int, to: int) -> int:
	var i := from
	while i < to and _is_hex_or_word_char(text[i]):
		i += 1
	return maxi(i, from + 1) if from < to else to

func _scan_string(text: String, from: int, to: int) -> int:
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

func _scan_number(text: String, from: int, to: int) -> int:
	var i := from
	if i < to and text[i] == "-":
		i += 1
	while i < to and (_is_digit(text[i]) or text[i] == "."):
		i += 1
	# Units: %, dp, px, em, rem, vh, vw, ...
	if i < to and text[i] == "%":
		return i + 1
	while i < to and text[i].to_lower() in "abcdefghijklmnopqrstuvwxyz":
		i += 1
	return i

func _is_word_char(c: String) -> bool:
	return c == "-" or c == "_" or (c.to_lower() >= "a" and c.to_lower() <= "z") or _is_digit(c)

func _is_hex_or_word_char(c: String) -> bool:
	return _is_word_char(c)

func _is_digit(c: String) -> bool:
	return c >= "0" and c <= "9"
