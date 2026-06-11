@tool
class_name RmlSyntaxHighlighter
extends RmlUiSyntaxHighlighterBase
## Syntax highlighting for .rml files in the script editor.
##
## XML structure (tags, attributes, comments, entities) plus RmlUi extras:
## - {{ expression }} data binding in text nodes
## - data-* / on* attributes get a distinct color
## - <style> blocks and style="" attributes delegate to RcssTokenizer
## - <script> blocks get basic GDScript coloring (forward-compatible with
##   inline GDScript support)

# --- Colors (Catppuccin Mocha-inspired, shared palette with RcssTokenizer) ---
const COLOR_TEXT      := Color(0.804, 0.839, 0.957)  # text — body text, = >
const COLOR_TAG       := Color(0.537, 0.706, 0.980)  # blue — tag names
const COLOR_BRACKET   := Color(0.580, 0.612, 0.722)  # overlay2 — < > /
const COLOR_ATTR      := Color(0.839, 0.757, 0.886)  # lavender — attribute names
const COLOR_DATA_ATTR := Color(0.804, 0.576, 0.969)  # mauve — data-*, on* attrs
const COLOR_STRING    := Color(0.647, 0.890, 0.631)  # green — attribute values
const COLOR_COMMENT   := Color(0.424, 0.443, 0.529)  # overlay1 — <!-- -->
const COLOR_EXPR      := Color(0.976, 0.702, 0.529)  # peach — {{ expr }}, &entity;
const COLOR_KEYWORD   := Color(0.804, 0.576, 0.969)  # mauve — gdscript keywords
const COLOR_NUMBER    := Color(0.976, 0.702, 0.529)  # peach — gdscript numbers

const GDSCRIPT_KEYWORDS := [
	"func", "var", "const", "enum", "class", "class_name", "extends", "signal",
	"if", "elif", "else", "for", "while", "match", "when", "break", "continue",
	"pass", "return", "await", "yield", "static", "and", "or", "not", "in", "is",
	"as", "self", "super", "true", "false", "null", "void", "breakpoint", "tool",
]

# --- Cross-line modes (low 4 bits of state; embedded sub-state above them) ---
enum Mode { TEXT, COMMENT, TAG, TAG_STYLE, TAG_SCRIPT, STYLE, SCRIPT }
const MODE_MASK := 0x0F
const SUB_SHIFT := 4

var _rcss := RcssTokenizer.new()


func _get_name() -> String:
	return "RML"


func _get_supported_languages() -> PackedStringArray:
	return PackedStringArray(["rml"])


func _tokenize_line(text: String, entry_state: int) -> Dictionary:
	var tokens := {}
	var mode := entry_state & MODE_MASK
	var sub := entry_state >> SUB_SHIFT
	var last_attr := ""
	var i := 0
	var n := text.length()

	while i < n:
		match mode:
			Mode.COMMENT:
				tokens[i] = {"color": COLOR_COMMENT}
				var close := text.find("-->", i)
				if close == -1:
					i = n
				else:
					i = close + 3
					mode = Mode.TEXT
					if i < n:
						tokens[i] = {"color": COLOR_TEXT}

			Mode.STYLE:
				var close := text.findn("</style", i)
				var seg_end := n if close == -1 else close
				if seg_end > i:
					var r := _rcss.tokenize(text, i, seg_end, sub)
					tokens.merge(r.tokens)
					sub = r.state
				i = seg_end
				if close != -1:
					mode = Mode.TEXT  # TEXT branch renders the closing tag
					sub = 0

			Mode.SCRIPT:
				var close := text.findn("</script", i)
				var seg_end := n if close == -1 else close
				if seg_end > i:
					_tokenize_gdscript(text, i, seg_end, tokens)
				i = seg_end
				if close != -1:
					mode = Mode.TEXT
					sub = 0

			Mode.TEXT:
				var c := text[i]
				if c == "<":
					if text.substr(i, 4) == "<!--":
						mode = Mode.COMMENT
						continue
					tokens[i] = {"color": COLOR_BRACKET}
					var j := i + 1
					var is_closing := j < n and text[j] == "/"
					if is_closing or (j < n and (text[j] == "!" or text[j] == "?")):
						j += 1
					var name_start := j
					j = _scan_name(text, j, n)
					var tag := text.substr(name_start, j - name_start).to_lower()
					if j > name_start:
						tokens[name_start] = {"color": COLOR_TAG}
					i = j
					if tag == "style" and not is_closing:
						mode = Mode.TAG_STYLE
					elif tag == "script" and not is_closing:
						mode = Mode.TAG_SCRIPT
					else:
						mode = Mode.TAG
				elif c == "{" and i + 1 < n and text[i + 1] == "{":
					tokens[i] = {"color": COLOR_EXPR}
					var close := text.find("}}", i + 2)
					i = n if close == -1 else close + 2
					if i < n:
						tokens[i] = {"color": COLOR_TEXT}
				elif c == "&":
					var semi := text.find(";", i)
					if semi != -1 and semi - i <= 9:
						tokens[i] = {"color": COLOR_EXPR}
						i = semi + 1
						if i < n:
							tokens[i] = {"color": COLOR_TEXT}
					else:
						i += 1
				else:
					i += 1

			_:  # Mode.TAG, Mode.TAG_STYLE, Mode.TAG_SCRIPT — inside <...>
				var c := text[i]
				if c == ">":
					tokens[i] = {"color": COLOR_BRACKET}
					i += 1
					if mode == Mode.TAG_STYLE:
						mode = Mode.STYLE
						sub = 0
					elif mode == Mode.TAG_SCRIPT:
						mode = Mode.SCRIPT
						sub = 0
					else:
						mode = Mode.TEXT
						if i < n:
							tokens[i] = {"color": COLOR_TEXT}
				elif c == "/" and i + 1 < n and text[i + 1] == ">":
					tokens[i] = {"color": COLOR_BRACKET}
					i += 2
					mode = Mode.TEXT  # self-closing — never enters STYLE/SCRIPT
					if i < n:
						tokens[i] = {"color": COLOR_TEXT}
				elif c == "\"" or c == "'":
					tokens[i] = {"color": COLOR_STRING}
					var send := _scan_string(text, i, n)
					if last_attr == "style" and send - i > 2:
						# Inline RCSS — declarations only, no selectors.
						var r := _rcss.tokenize(text, i + 1, send - 1, RcssTokenizer.STATE_DECLARATIONS)
						tokens.merge(r.tokens)
						tokens[send - 1] = {"color": COLOR_STRING}
					i = send
				elif c == "=":
					tokens[i] = {"color": COLOR_TEXT}
					i += 1
				elif _is_name_char(c):
					var e := _scan_name(text, i, n)
					last_attr = text.substr(i, e - i).to_lower()
					var is_dynamic := last_attr.begins_with("data-") or last_attr.begins_with("on")
					tokens[i] = {"color": COLOR_DATA_ATTR if is_dynamic else COLOR_ATTR}
					i = e
				else:
					i += 1

	return {"tokens": tokens, "state": mode | (sub << SUB_SHIFT)}


## Minimal GDScript coloring for <script> block segments — keywords, strings,
## comments, and numbers. Not full fidelity; just enough to read.
func _tokenize_gdscript(text: String, from: int, to: int, tokens: Dictionary) -> void:
	var i := from
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
			tokens[i] = {"color": COLOR_KEYWORD if word in GDSCRIPT_KEYWORDS else COLOR_TEXT}
			i = e
		else:
			i += 1


func _scan_name(text: String, from: int, to: int) -> int:
	var i := from
	while i < to and (_is_name_char(text[i]) or (text[i] >= "0" and text[i] <= "9")):
		i += 1
	return i


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


func _is_name_char(c: String) -> bool:
	var l := c.to_lower()
	return (l >= "a" and l <= "z") or c == "-" or c == "_" or c == ":"
