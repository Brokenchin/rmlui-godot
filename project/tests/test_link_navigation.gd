extends SceneTree
## Unit tests for RmlLinkNavigation's pure helpers (issue #23): extracting the
## link path under the caret and resolving it the way the runtime loader does.
## The Ctrl-click wiring itself is editor-only (is_editor_hint can't be forced
## from `-s`) and is verified by hand in the script editor.

const Nav := preload("res://addons/rmlui-godot/editor/rml_link_navigation.gd")

var _fails := 0


func _initialize() -> void:
	_test_path_token_at()
	_test_resolve_path()
	_test_suffix_match()
	print("ALL PASSED" if _fails == 0 else "%d FAILED" % _fails)
	quit(_fails)


func _test_path_token_at() -> void:
	# Caret anywhere inside the value returns the whole path; outside returns "".
	var link := "<link type=\"text/rcss\" href=\"../theme/theme.rcss\" />"
	_eq("href value", Nav.path_token_at(link, link.find("theme.rcss")), "../theme/theme.rcss")
	_eq("type attr is not a link", Nav.path_token_at(link, link.find("text/rcss")), "")

	var scr := "<script src=\"res://ui/main.gd\"></script>"
	_eq("src value", Nav.path_token_at(scr, scr.find("main.gd")), "res://ui/main.gd")

	var single := "<link href='base.rcss'>"
	_eq("single-quoted href", Nav.path_token_at(single, single.find("base")), "base.rcss")

	var imp := "@import \"reset.rcss\";"
	_eq("@import value", Nav.path_token_at(imp, imp.find("reset")), "reset.rcss")

	var urlq := "background: url(\"img/bg.png\");"
	_eq("url() quoted", Nav.path_token_at(urlq, urlq.find("bg.png")), "img/bg.png")

	var urlb := "decorator: image(skin/panel.png);"  # not url(), no match
	_eq("non-url paren ignored", Nav.path_token_at(urlb, urlb.find("panel")), "")

	var urlbare := "background: url(img/bg.png);"
	_eq("url() bare", Nav.path_token_at(urlbare, urlbare.find("bg.png")), "img/bg.png")

	_eq("empty line", Nav.path_token_at("", 0), "")
	_eq("plain text", Nav.path_token_at("just some text", 4), "")


func _test_resolve_path() -> void:
	var doc := "res://ui/views/frame.rml"
	_eq("relative ../ joins + simplifies",
		Nav.resolve_path("../theme/theme.rcss", doc), "res://ui/theme/theme.rcss")
	_eq("bare relative joins doc dir",
		Nav.resolve_path("frame.gd", doc), "res://ui/views/frame.gd")
	_eq("absolute res:// passes through",
		Nav.resolve_path("res://shared/base.rcss", doc), "res://shared/base.rcss")
	_eq("user:// passes through",
		Nav.resolve_path("user://save.cfg", doc), "user://save.cfg")
	_eq("leading slash -> res://",
		Nav.resolve_path("/ui/theme.rcss", doc), "res://ui/theme.rcss")
	_eq("pipe-encoded scheme decoded",
		Nav.resolve_path("res|//a/b.gd", doc), "res://a/b.gd")
	_eq("empty raw -> empty", Nav.resolve_path("", doc), "")
	_eq("whitespace raw -> empty", Nav.resolve_path("   ", doc), "")


func _test_suffix_match() -> void:
	# Fallback used when the document's own path is unknown: match a project file
	# by the link's tail, preferring the longest relative-path match.
	var files := PackedStringArray([
		"res://ui/theme/theme.rcss",
		"res://other/theme.rcss",
		"res://ui/views/frame.rml",
	])
	_eq("full tail beats basename",
		Nav.best_suffix_match(files, "../theme/theme.rcss"), "res://ui/theme/theme.rcss")
	_eq("basename fallback",
		Nav.best_suffix_match(files, "frame.rml"), "res://ui/views/frame.rml")
	_eq("res:// tail match",
		Nav.best_suffix_match(files, "res://ui/theme/theme.rcss"), "res://ui/theme/theme.rcss")
	_eq("no match -> empty",
		Nav.best_suffix_match(files, "missing.gd"), "")
	_eq("empty raw -> empty", Nav.best_suffix_match(files, "  "), "")


func _eq(name: String, got: String, want: String) -> void:
	var ok := got == want
	print("  %s  %s (got '%s', want '%s')" % ["PASS" if ok else "FAIL", name, got, want])
	if not ok:
		_fails += 1
