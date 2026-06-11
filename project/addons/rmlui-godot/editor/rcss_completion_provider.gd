@tool
class_name RcssCompletionProvider
extends RefCounted
## Code completion for RCSS — property names and per-property value keywords.
##
## Attached to script-editor CodeEdits whose syntax highlighter is RCSS or RML
## (see rmlui_editor_plugin.gd). Trigger with Ctrl+Space; typing ':' also
## requests value completion via code_completion_prefixes.
##
## Data source: static tables below (MVP). Follow-up: export the registered
## property list from C++ via Rml::StyleSheetSpecification so this can never
## drift from the engine.

const COMMON_VALUES := ["inherit"]

# property -> value keywords ([] = freeform: lengths, colors, ...)
const PROPERTIES := {
	"display": ["block", "inline", "inline-block", "flow-root", "flex", "table", "table-row", "table-cell", "none"],
	"position": ["static", "relative", "absolute", "fixed"],
	"top": [], "right": [], "bottom": [], "left": [],
	"float": ["none", "left", "right"],
	"clear": ["none", "left", "right", "both"],
	"z-index": ["auto"],
	"width": ["auto"], "height": ["auto"],
	"min-width": [], "min-height": [], "max-width": ["none"], "max-height": ["none"],
	"box-sizing": ["content-box", "border-box"],
	"margin": ["auto"], "margin-top": ["auto"], "margin-right": ["auto"], "margin-bottom": ["auto"], "margin-left": ["auto"],
	"padding": [], "padding-top": [], "padding-right": [], "padding-bottom": [], "padding-left": [],
	"border": [], "border-width": [],
	"border-top-width": [], "border-right-width": [], "border-bottom-width": [], "border-left-width": [],
	"border-color": [],
	"border-top-color": [], "border-right-color": [], "border-bottom-color": [], "border-left-color": [],
	"border-radius": [],
	"border-top-left-radius": [], "border-top-right-radius": [], "border-bottom-left-radius": [], "border-bottom-right-radius": [],
	"overflow": ["visible", "hidden", "auto", "scroll"],
	"overflow-x": ["visible", "hidden", "auto", "scroll"],
	"overflow-y": ["visible", "hidden", "auto", "scroll"],
	"clip": ["auto", "none", "always"],
	"visibility": ["visible", "hidden"],
	"opacity": [],
	"color": [],
	"background-color": [],
	"image-color": [],
	"caret-color": [],
	"font-family": [],
	"font-size": [],
	"font-style": ["normal", "italic"],
	"font-weight": ["normal", "bold"],
	"line-height": [],
	"letter-spacing": ["normal"],
	"text-align": ["left", "right", "center", "justify"],
	"text-decoration": ["none", "underline", "overline", "line-through"],
	"text-transform": ["none", "capitalize", "uppercase", "lowercase"],
	"vertical-align": ["baseline", "middle", "sub", "super", "text-top", "text-bottom", "top", "bottom", "center"],
	"white-space": ["normal", "pre", "nowrap", "pre-wrap", "pre-line"],
	"word-break": ["normal", "break-all", "break-word"],
	"cursor": [],
	"pointer-events": ["auto", "none"],
	"focus": ["none", "auto"],
	"tab-index": ["none", "auto"],
	"drag": ["none", "drag", "drag-drop", "block", "clone"],
	"nav-up": ["none", "auto", "horizontal", "vertical"],
	"nav-down": ["none", "auto", "horizontal", "vertical"],
	"nav-left": ["none", "auto", "horizontal", "vertical"],
	"nav-right": ["none", "auto", "horizontal", "vertical"],
	"scrollbar-margin": [],
	"decorator": ["image", "tiled-horizontal", "tiled-vertical", "tiled-box", "ninepatch", "gradient", "horizontal-gradient", "vertical-gradient", "linear-gradient", "radial-gradient", "conic-gradient", "shader", "none"],
	"font-effect": ["glow", "outline", "shadow", "blur", "none"],
	"filter": ["blur", "brightness", "contrast", "drop-shadow", "grayscale", "hue-rotate", "invert", "opacity", "saturate", "sepia", "none"],
	"backdrop-filter": ["none"],
	"mask-image": ["none"],
	"transition": ["all", "none"],
	"animation": ["none"],
	"transform": ["none", "translate", "translateX", "translateY", "rotate", "scale", "scaleX", "scaleY", "skew", "skewX", "skewY", "matrix"],
	"transform-origin": ["left", "center", "right", "top", "bottom"],
	"perspective": ["none"],
	"perspective-origin": ["left", "center", "right", "top", "bottom"],
	"flex-direction": ["row", "row-reverse", "column", "column-reverse"],
	"flex-wrap": ["nowrap", "wrap", "wrap-reverse"],
	"flex-flow": ["row", "column", "nowrap", "wrap"],
	"justify-content": ["flex-start", "flex-end", "center", "space-between", "space-around", "space-evenly"],
	"align-items": ["flex-start", "flex-end", "center", "baseline", "stretch"],
	"align-content": ["flex-start", "flex-end", "center", "space-between", "space-around", "stretch"],
	"align-self": ["auto", "flex-start", "flex-end", "center", "baseline", "stretch"],
	"flex": ["none", "auto"],
	"flex-grow": [], "flex-shrink": [], "flex-basis": ["auto"],
	"gap": [], "row-gap": [], "column-gap": [],
	"fill-image": [],
	"overscroll-behavior": ["auto", "contain", "none"],
}

const COLOR_PROPERTIES := [
	"color", "background-color", "border-color", "image-color", "caret-color",
	"border-top-color", "border-right-color", "border-bottom-color", "border-left-color",
]

const COLOR_NAMES := [
	"black", "white", "red", "green", "blue", "yellow", "orange", "purple",
	"aqua", "fuchsia", "gray", "grey", "lime", "maroon", "navy", "olive",
	"silver", "teal", "transparent",
]

# --- RML tag/attribute data ---

const RML_TAGS := [
	"rml", "head", "title", "link", "meta", "style", "script", "template", "body",
	"div", "span", "p", "h1", "h2", "h3", "h4", "h5", "h6", "em", "strong", "br", "hr",
	"img", "button", "input", "textarea", "select", "option", "label", "form",
	"table", "thead", "tbody", "tfoot", "tr", "td", "th", "col", "colgroup",
	"ul", "ol", "li", "tabset", "tab", "panel", "progress", "handle",
]

const GLOBAL_ATTRIBUTES := [
	"id", "class", "style",
	"data-model", "data-if", "data-visible", "data-for", "data-rml", "data-value",
	"data-checked", "data-alias", "data-attr-", "data-style-", "data-class-", "data-event-",
]

const EVENT_ATTRIBUTES := [
	"onclick", "ondblclick", "onmousedown", "onmouseup", "onmouseover", "onmouseout",
	"onmousemove", "onmousescroll", "onkeydown", "onkeyup", "ontextinput",
	"onfocus", "onblur", "onload", "onunload", "onshow", "onhide", "onresize",
	"onscroll", "onchange", "onsubmit", "ontabchange", "onanimationend", "ontransitionend",
	"ondragstart", "ondrag", "ondragend", "ondragdrop", "ondragover", "ondragout", "ondragmove",
]

const TAG_ATTRIBUTES := {
	"rml": [],
	"link": ["rel", "href", "type"],
	"script": ["type", "src"],
	"template": ["name", "content", "src"],
	"img": ["src", "sprite", "rect", "width", "height"],
	"input": ["type", "name", "value", "checked", "disabled", "min", "max", "step", "maxlength", "size"],
	"textarea": ["name", "rows", "cols", "wrap", "maxlength"],
	"select": ["name", "value"],
	"option": ["value", "selected", "disabled"],
	"label": ["for"],
	"form": ["onsubmit"],
	"handle": ["move_target", "size_target", "edge_margin"],
	"progress": ["value", "max", "direction", "start-edge", "fill-image"],
	"tab": ["tabindex"],
	"panel": ["tabindex"],
	"td": ["colspan", "rowspan"],
	"th": ["colspan", "rowspan"],
	"col": ["span"],
	"colgroup": ["span"],
	"body": ["template"],
}

# Properties exported by the running RmlUi build (authoritative). Lazily
# fetched; static PROPERTIES is the fallback and supplies value keywords.
var _engine_properties := PackedStringArray()


## Fill completion options on `ce` based on caret context. Returns true when
## options were added (caller then shows the popup).
func fill_options(ce: CodeEdit) -> bool:
	var line := ce.get_line(ce.get_caret_line())
	var col := ce.get_caret_column()
	var before := line.substr(0, col)

	# In RML, only complete inside a style="..." attribute or <style> block —
	# the plugin only attaches us to RCSS-highlighted tabs and RML tabs, so for
	# RML check we're inside a style context cheaply.
	var colon := before.rfind(":")
	var semi := before.rfind(";")
	var brace := before.rfind("{")

	if colon > semi and colon > brace:
		# Value position — keywords for the property before the colon.
		var prop := _property_before_colon(before, colon)
		var values: Array = PROPERTIES.get(prop, [])
		for v in values:
			ce.add_code_completion_option(CodeEdit.KIND_CONSTANT, v, v)
		if prop in COLOR_PROPERTIES:
			for c in COLOR_NAMES:
				ce.add_code_completion_option(CodeEdit.KIND_CONSTANT, c, c)
		for v in COMMON_VALUES:
			ce.add_code_completion_option(CodeEdit.KIND_CONSTANT, v, v)
		return not values.is_empty() or prop in COLOR_PROPERTIES
	else:
		# Property-name position — engine-registered names first, static
		# table as fallback (some names may only exist in one source).
		var seen := {}
		for prop in _get_engine_properties():
			seen[prop] = true
			ce.add_code_completion_option(CodeEdit.KIND_MEMBER, prop, prop + ": ")
		for prop in PROPERTIES:
			if not seen.has(prop):
				ce.add_code_completion_option(CodeEdit.KIND_MEMBER, prop, prop + ": ")
		return true


## Completion for .rml buffers: tag names, per-tag attributes, closing tags,
## RCSS inside <style> blocks, GDScript inside <script> blocks.
func fill_rml_options(ce: CodeEdit) -> bool:
	var caret_line := ce.get_caret_line()
	var before := ce.get_line(caret_line).substr(0, ce.get_caret_column())
	var txt := _text_up_to_caret(ce, caret_line, before)

	# Inside a <style> block (its opening tag closed, no </style> yet)?
	var style_open := txt.rfindn("<style")
	var style_close := txt.rfindn("</style")
	var lt := txt.rfind("<")
	var gt := txt.rfind(">")
	if style_open > style_close and gt > style_open and lt <= style_open:
		return fill_options(ce)

	# Inside a <script> block → GDScript completion.
	var script_open := txt.rfindn("<script")
	var script_close := txt.rfindn("</script")
	if script_open > script_close and gt > script_open and lt <= script_open:
		return fill_gdscript_options(ce, before)

	if lt <= gt:
		return false  # text content — completion starts at '<'

	# Inside a tag. Scan it to find: tag name, and whether the caret sits in
	# the name, an attribute position, or a quoted attribute value.
	var tag_part := txt.substr(lt)
	var is_closing := tag_part.length() > 1 and tag_part[1] == "/"
	var i := 2 if is_closing else 1
	var n := tag_part.length()
	var name_end := i
	while name_end < n and _is_tag_char(tag_part[name_end]):
		name_end += 1
	var tag := tag_part.substr(i, name_end - i).to_lower()

	if name_end == n:
		# Still typing the tag name. NOTE: CodeEdit's completion word scanner
		# includes '/' in the matched word, so after "</di" the filter base is
		# "/di" — closing-tag options must carry the leading '/' to survive
		# filtering (and their insert text replaces it correctly).
		if is_closing:
			# Best match first: the innermost currently-unclosed tag.
			var open_tag := _innermost_open_tag(txt.substr(0, lt))
			if not open_tag.is_empty():
				ce.add_code_completion_option(CodeEdit.KIND_CLASS, "/" + open_tag, "/" + open_tag + ">",
					Color(0.647, 0.890, 0.631))
			for t in RML_TAGS:
				if t != open_tag:
					ce.add_code_completion_option(CodeEdit.KIND_CLASS, "/" + t, "/" + t + ">")
		else:
			for t in RML_TAGS:
				ce.add_code_completion_option(CodeEdit.KIND_CLASS, t, t)
		return true

	# Walk attributes to find quote state and the attribute the caret is in.
	i = name_end
	var in_quote := ""
	var attr := ""
	var current_attr := ""
	while i < n:
		var c := tag_part[i]
		if in_quote != "":
			if c == in_quote:
				in_quote = ""
			i += 1
			continue
		if c == "\"" or c == "'":
			in_quote = c
			attr = current_attr
			i += 1
			continue
		if _is_tag_char(c) or c == "-":
			var e := i
			while e < n and (_is_tag_char(tag_part[e]) or tag_part[e] == "-"):
				e += 1
			current_attr = tag_part.substr(i, e - i).to_lower()
			i = e
			continue
		i += 1

	if in_quote != "":
		# Inside an attribute value.
		if attr == "style":
			return fill_options(ce)  # colon/semicolon heuristic works inline
		return false

	# Attribute-name position.
	var attrs: Array = TAG_ATTRIBUTES.get(tag, [])
	for a in attrs:
		ce.add_code_completion_option(CodeEdit.KIND_MEMBER, a, a + "=\"")
	for a in GLOBAL_ATTRIBUTES:
		ce.add_code_completion_option(CodeEdit.KIND_MEMBER, a, a if a.ends_with("-") else a + "=\"")
	for a in EVENT_ATTRIBUTES:
		ce.add_code_completion_option(CodeEdit.KIND_SIGNAL, a, a + "=\"")
	return true


## Auto-close support: when the caret sits right after a '>' that completes an
## opening tag, returns the tag name to close ("" when nothing should happen).
## RmlUi is XML — closing tags must be named (</> is not a thing) — so editors
## conventionally insert the close pair the moment the opening tag is finished.
static func autoclose_tag_for(line_text: String, col: int) -> String:
	if col < 2 or col > line_text.length() or line_text[col - 1] != ">":
		return ""
	if line_text[col - 2] == "/":
		return ""  # self-closing <br/>
	var lt := line_text.rfind("<", col - 1)
	if lt == -1:
		return ""
	var tag_part := line_text.substr(lt, col - lt)
	# Opening tag only: <name> or <name attr="..."> — rejects </close>,
	# <!-- comments, <?xml, and quote-aware so > inside strings can't end it.
	var re := RegEx.create_from_string("^<([A-Za-z][A-Za-z0-9_-]*)(\\s(?:\"[^\"]*\"|'[^']*'|[^>\"'])*)?>$")
	var m := re.search(tag_part)
	if m == null:
		return ""
	var tag := m.get_string(1)
	if tag.to_lower() in VOID_TAGS:
		return ""
	# Don't double-insert when the close pair is already there.
	if line_text.substr(col).begins_with("</" + tag + ">"):
		return ""
	return tag


const VOID_TAGS := ["br", "hr", "img", "input", "link", "meta", "col"]


## The innermost tag opened but not yet closed before `txt`'s end — the best
## suggestion when the user types "</".
func _innermost_open_tag(txt: String) -> String:
	var stack: Array[String] = []
	var re := RegEx.create_from_string("<(/?)([A-Za-z][A-Za-z0-9_-]*)((?:\"[^\"]*\"|'[^']*'|[^>\"'])*)>")
	for m in re.search_all(txt):
		var closing := m.get_string(1) == "/"
		var tag := m.get_string(2).to_lower()
		var self_closing: bool = m.get_string(3).ends_with("/") or tag in VOID_TAGS
		if closing:
			# Pop to the matching open tag (tolerates mismatches).
			var idx := stack.rfind(tag)
			if idx >= 0:
				stack.resize(idx)
		elif not self_closing:
			stack.append(tag)
	return stack.back() if not stack.is_empty() else ""


# --- GDScript completion inside <script> blocks ---
#
# Godot's semantic completion engine isn't exposed to plugins, so this is
# reflection-based: member access on a known type lists its real methods,
# properties and signals via ClassDB; bare identifiers get keywords, the
# block's own funcs/vars, engine class names and the rml_context/event
# conventions. Accurate because it's reflection, not a hardcoded list.

const GDSCRIPT_KEYWORDS := [
	"func", "var", "const", "signal", "enum", "class", "extends",
	"if", "elif", "else", "for", "while", "match", "when", "break", "continue",
	"pass", "return", "await", "static", "and", "or", "not", "in", "is", "as",
	"self", "super", "true", "false", "null", "print", "preload", "load",
	"range", "str", "int", "float", "bool", "abs", "clamp", "lerp", "weakref",
]

## `before` = current line up to the caret.
func fill_gdscript_options(ce: CodeEdit, before: String) -> bool:
	# Member access: identifier "." [partial-word]
	var m := RegEx.create_from_string("([A-Za-z_][A-Za-z0-9_]*)\\.[A-Za-z0-9_]*$").search(before)
	if m:
		var base := m.get_string(1)
		var cls := ""
		if base == "rml_context":
			cls = "RmlContext"
		elif ClassDB.class_exists(base):
			cls = base  # engine singletons share their class name: Input, OS, Time, Engine…
		if cls.is_empty():
			return false
		for opt in class_member_options(cls):
			ce.add_code_completion_option(opt[0], opt[1], opt[2])
		return true

	# Bare identifier position.
	for kw in GDSCRIPT_KEYWORDS:
		ce.add_code_completion_option(CodeEdit.KIND_PLAIN_TEXT, kw, kw)
	ce.add_code_completion_option(CodeEdit.KIND_VARIABLE, "rml_context", "rml_context")
	ce.add_code_completion_option(CodeEdit.KIND_FUNCTION, "_on_input_action", "_on_input_action(action: String, pressed: bool):")
	for local in _buffer_locals(ce.text):
		ce.add_code_completion_option(local[0], local[1], local[1])
	# Engine classes (popup filters as you type — Upper-case prefixes find these).
	for cls_name in ClassDB.get_class_list():
		ce.add_code_completion_option(CodeEdit.KIND_CLASS, cls_name, cls_name)
	return true


## [kind, display, insert] for every method/property/signal of `cls`,
## inherited members included.
static func class_member_options(cls: String) -> Array:
	var out := []
	for sig in ClassDB.class_get_signal_list(cls):
		out.append([CodeEdit.KIND_SIGNAL, sig.name, sig.name])
	for prop in ClassDB.class_get_property_list(cls):
		# Skip group/category markers and internals.
		if prop.usage & (PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_INTERNAL):
			continue
		if String(prop.name).contains("/"):
			continue
		out.append([CodeEdit.KIND_MEMBER, prop.name, prop.name])
	for method in ClassDB.class_get_method_list(cls):
		var args: Array = method.args
		var display: String = method.name + "(" + ", ".join(args.map(func(a): return String(a.name))) + ")"
		var insert: String = method.name + ("()" if args.is_empty() else "(")
		out.append([CodeEdit.KIND_FUNCTION, display, insert])
	return out


## funcs/vars/signals declared in the buffer itself.
func _buffer_locals(text: String) -> Array:
	var out := []
	for m in RegEx.create_from_string("(?m)^\\s*(func|var|const|signal)\\s+([A-Za-z_][A-Za-z0-9_]*)").search_all(text):
		var kind := CodeEdit.KIND_FUNCTION if m.get_string(1) == "func" else \
			(CodeEdit.KIND_SIGNAL if m.get_string(1) == "signal" else CodeEdit.KIND_VARIABLE)
		out.append([kind, m.get_string(2)])
	return out


func _text_up_to_caret(ce: CodeEdit, caret_line: int, before: String) -> String:
	# Join enough preceding lines for multi-line tags / <style> blocks.
	var start := maxi(0, caret_line - 200)
	var parts := PackedStringArray()
	for i in range(start, caret_line):
		parts.append(ce.get_line(i))
	parts.append(before)
	return "\n".join(parts)


func _get_engine_properties() -> PackedStringArray:
	if _engine_properties.is_empty() and Engine.has_singleton("RmlManager"):
		var mgr := Engine.get_singleton("RmlManager")
		if mgr.has_method("get_supported_rcss_properties"):
			# Empty until RmlUi initializes (first context) — retried until then.
			_engine_properties = mgr.get_supported_rcss_properties()
	return _engine_properties


func _is_tag_char(c: String) -> bool:
	var l := c.to_lower()
	return (l >= "a" and l <= "z") or (c >= "0" and c <= "9") or c == "_"


func _property_before_colon(before: String, colon: int) -> String:
	var i := colon - 1
	while i >= 0 and (before[i] == " " or before[i] == "\t"):
		i -= 1
	var end := i + 1
	while i >= 0 and (before[i] == "-" or before[i] == "_" or before[i].to_lower() >= "a" and before[i].to_lower() <= "z"):
		i -= 1
	return before.substr(i + 1, end - (i + 1))
