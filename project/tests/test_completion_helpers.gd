extends SceneTree
## Unit tests for RcssCompletionProvider tag-stack helper.

const Provider := preload("res://addons/rmlui-godot/editor/rcss_completion_provider.gd")


func _initialize() -> void:
	var p := Provider.new()
	var fails := 0

	fails += _check("innermost div", p._innermost_open_tag("<rml><body><div>"), "div")
	fails += _check("closed div -> body", p._innermost_open_tag("<rml><body><div>x</div>"), "body")
	fails += _check("self-closing ignored", p._innermost_open_tag("<body><img src=\"a.png\"/><span>"), "span")
	fails += _check("void tag ignored", p._innermost_open_tag("<body><br><p>"), "p")
	fails += _check("attr with > in quotes", p._innermost_open_tag("<body><div class=\"a>b\">"), "div")
	fails += _check("all closed", p._innermost_open_tag("<rml></rml>"), "")

	# --- autoclose_tag_for: '>' just typed at col completes an opening tag ---
	var t := "<div>"
	fails += _check("autoclose simple", Provider.autoclose_tag_for(t, t.length()), "div")
	t = "<div class=\"test\">"
	fails += _check("autoclose with attrs", Provider.autoclose_tag_for(t, t.length()), "div")
	t = "<div class=\"a>b\">"
	fails += _check("autoclose > inside quotes", Provider.autoclose_tag_for(t, t.length()), "div")
	t = "</div>"
	fails += _check("no autoclose on closing tag", Provider.autoclose_tag_for(t, t.length()), "")
	t = "<br/>"
	fails += _check("no autoclose on self-closing", Provider.autoclose_tag_for(t, t.length()), "")
	t = "<img src=\"a.png\">"
	fails += _check("no autoclose on void tag", Provider.autoclose_tag_for(t, t.length()), "")
	t = "<!-- comment -->"
	fails += _check("no autoclose on comment", Provider.autoclose_tag_for(t, t.length()), "")
	t = "<div></div>"
	fails += _check("no double insert", Provider.autoclose_tag_for(t, 5), "")
	t = "\t<span>"
	fails += _check("autoclose indented", Provider.autoclose_tag_for(t, t.length()), "span")
	t = "a > b"
	fails += _check("no autoclose on bare >", Provider.autoclose_tag_for(t, 3), "")

	# --- tag_has_matching_close: suppress auto-close when already balanced (#35) ---
	# Issue #35 repro: re-typing '>' on a complete <div ...> with children — the
	# text after the caret already closes the element, so DON'T add another.
	fails += _checkb("balanced div with children",
		Provider.tag_has_matching_close("<div class=\"box\"><span>x</span></div></div>", "div"), true)
	# Fresh tag at end of buffer — nothing closes it yet, so DO auto-close.
	fails += _checkb("unclosed div", Provider.tag_has_matching_close("\n  text", "div"), false)
	# A new <div> typed above an existing balanced <div> stays unclosed.
	fails += _checkb("new div above balanced one",
		Provider.tag_has_matching_close("\n<div>existing</div>\n", "div"), false)
	# Nested same-name tags: the outer open matches the LAST close.
	fails += _checkb("nested divs balanced",
		Provider.tag_has_matching_close("<div></div></div>", "div"), true)
	# Name boundary: "<divider>" must not count as a "div" close/open.
	fails += _checkb("divider is not div",
		Provider.tag_has_matching_close("<divider></divider>text", "div"), false)
	# A nested open whose attribute value contains '>' must still be counted as
	# one open (the quoted '>' can't end the tag early), so the element stays
	# balanced. text_after excludes the just-typed tag itself (the depth-1 seed).
	fails += _checkb("gt inside nested attr value",
		Provider.tag_has_matching_close("<div title=\"a>b\">x</div></div>", "div"), true)

	print("ALL PASSED" if fails == 0 else "%d FAILED" % fails)
	quit(fails)


func _check(name: String, got: String, want: String) -> int:
	var ok := got == want
	print("  %s  %s (got '%s', want '%s')" % ["PASS" if ok else "FAIL", name, got, want])
	return 0 if ok else 1


func _checkb(name: String, got: bool, want: bool) -> int:
	var ok := got == want
	print("  %s  %s (got %s, want %s)" % ["PASS" if ok else "FAIL", name, got, want])
	return 0 if ok else 1
