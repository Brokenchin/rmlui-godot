extends SceneTree
## Headless regression test for RcssTokenizer.
## Run: godot --headless -s tests/test_rcss_tokenizer.gd (from project/)

var _fails := 0


func _init() -> void:
	var t := RcssTokenizer.new()

	# Selector line: 'div {' — selector colored, exit state enters block.
	var r := t.tokenize("div {", 0, 5, 0)
	_check("selector token", r.tokens.get(0, {}).get("color") == RcssTokenizer.COLOR_SELECTOR)
	_check("open brace enters block", r.state & RcssTokenizer.DEPTH_MASK == 1)

	# Declaration on its own line (the old per-line highlighter got this wrong).
	r = t.tokenize("\tcolor: red;", 0, 12, 1)
	_check("property color", r.tokens.get(1, {}).get("color") == RcssTokenizer.COLOR_PROPERTY)
	_check("value color", r.tokens.get(8, {}).get("color") == RcssTokenizer.COLOR_VALUE)
	_check("stays in block", r.state & RcssTokenizer.DEPTH_MASK == 1)
	_check("value flag cleared by ;", r.state & RcssTokenizer.FLAG_VALUE == 0)

	# Multi-line comment: state carries across lines.
	r = t.tokenize("/* start of comment", 0, 19, 0)
	_check("comment opens", r.state & RcssTokenizer.FLAG_COMMENT != 0)
	r = t.tokenize("still comment */ div", 0, 20, r.state)
	_check("comment continuation colored", r.tokens.get(0, {}).get("color") == RcssTokenizer.COLOR_COMMENT)
	_check("comment closes", r.state & RcssTokenizer.FLAG_COMMENT == 0)
	_check("selector after comment", r.tokens.get(17, {}).get("color") == RcssTokenizer.COLOR_SELECTOR)

	# Closing brace returns to selector context.
	r = t.tokenize("}", 0, 1, 1)
	_check("close brace exits block", r.state & RcssTokenizer.DEPTH_MASK == 0)

	# style="" attribute mode: declarations without selectors.
	var decl := "width: 50px; height: 2em"
	r = t.tokenize(decl, 0, decl.length(), RcssTokenizer.STATE_DECLARATIONS)
	_check("decl-mode property", r.tokens.get(0, {}).get("color") == RcssTokenizer.COLOR_PROPERTY)
	_check("decl-mode number", r.tokens.get(7, {}).get("color") == RcssTokenizer.COLOR_NUMBER)
	_check("decl-mode second property", r.tokens.get(13, {}).get("color") == RcssTokenizer.COLOR_PROPERTY)

	# Nested selector inside @media block is NOT a property.
	r = t.tokenize("div:hover {", 0, 11, 1)
	_check("nested selector", r.tokens.get(0, {}).get("color") == RcssTokenizer.COLOR_SELECTOR)
	_check("nested pseudo", r.tokens.get(3, {}).get("color") == RcssTokenizer.COLOR_PSEUDO)
	_check("nested depth", r.state & RcssTokenizer.DEPTH_MASK == 2)

	# Hex color literal in value vs #id selector.
	r = t.tokenize("color: #ff00ff;", 0, 15, 1)
	_check("hex literal in value", r.tokens.get(7, {}).get("color") == RcssTokenizer.COLOR_COLOR_LIT)
	r = t.tokenize("#main {", 0, 7, 0)
	_check("id selector", r.tokens.get(0, {}).get("color") == RcssTokenizer.COLOR_ID)

	# !important and quoted strings.
	r = t.tokenize("font-family: \"Noto Sans\" !important;", 0, 36, 1)
	_check("string in value", r.tokens.get(13, {}).get("color") == RcssTokenizer.COLOR_STRING)
	_check("important", r.tokens.get(25, {}).get("color") == RcssTokenizer.COLOR_IMPORTANT)

	# @-rule at top level.
	r = t.tokenize("@media (min-width: 600dp) {", 0, 27, 0)
	_check("at-rule", r.tokens.get(0, {}).get("color") == RcssTokenizer.COLOR_AT_RULE)
	_check("at-rule enters block", r.state & RcssTokenizer.DEPTH_MASK == 1)

	if _fails == 0:
		print("ALL TESTS PASSED")
	else:
		print("%d TEST(S) FAILED" % _fails)
	quit(_fails)


func _check(name: String, ok: bool) -> void:
	if ok:
		print("  PASS  %s" % name)
	else:
		print("  FAIL  %s" % name)
		_fails += 1
