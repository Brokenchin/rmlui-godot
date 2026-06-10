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
		# Property-name position.
		for prop in PROPERTIES:
			ce.add_code_completion_option(CodeEdit.KIND_MEMBER, prop, prop + ": ")
		return true


func _property_before_colon(before: String, colon: int) -> String:
	var i := colon - 1
	while i >= 0 and (before[i] == " " or before[i] == "\t"):
		i -= 1
	var end := i + 1
	while i >= 0 and (before[i] == "-" or before[i] == "_" or before[i].to_lower() >= "a" and before[i].to_lower() <= "z"):
		i -= 1
	return before.substr(i + 1, end - (i + 1))
