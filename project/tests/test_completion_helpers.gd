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

	print("ALL PASSED" if fails == 0 else "%d FAILED" % fails)
	quit(fails)


func _check(name: String, got: String, want: String) -> int:
	var ok := got == want
	print("  %s  %s (got '%s', want '%s')" % ["PASS" if ok else "FAIL", name, got, want])
	return 0 if ok else 1
