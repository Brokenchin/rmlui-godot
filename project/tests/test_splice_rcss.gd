extends SceneTree
## Unit test for RmlPreviewPanel._splice_live_rcss.

const PanelScript := preload("res://addons/rmlui-godot/editor/rml_preview_panel.gd")


func _initialize() -> void:
	var rml := """<rml>
<head>
	<link type="text/rcss" href="res://addons/rmlui-godot/base.rcss" />
	<link type="text/rcss" href="hello.rcss" />
	<style>
		#container { width: 400px; }
	</style>
</head>
<body><div id="container">x</div></body>
</rml>"""

	var fails := 0

	# Replaces the right link, keeps base.rcss, preserves position.
	var out: String = PanelScript._splice_live_rcss(rml, "hello.rcss", "#container { width: 111px; }")
	fails += _check("link replaced", not out.contains("href=\"hello.rcss\""), out)
	fails += _check("base.rcss kept", out.contains("base.rcss"), out)
	fails += _check("live css inserted", out.contains("width: 111px"), out)
	fails += _check("style block still after", out.find("width: 111px") < out.find("width: 400px"), out)

	# Unknown file -> empty (caller falls back to inject).
	out = PanelScript._splice_live_rcss(rml, "other.rcss", "x{}")
	fails += _check("unknown file falls back", out.is_empty(), out)

	# res:// full path in href also matches by file name.
	out = PanelScript._splice_live_rcss(rml, "base.rcss", "body{}")
	fails += _check("res:// href matched", not out.contains("base.rcss\""), out)

	print("ALL PASSED" if fails == 0 else "%d FAILED" % fails)
	quit(fails)


func _check(name: String, ok: bool, _ctx: String) -> int:
	print("  %s  %s" % ["PASS" if ok else "FAIL", name])
	return 0 if ok else 1
