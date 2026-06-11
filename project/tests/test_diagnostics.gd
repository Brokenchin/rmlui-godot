extends SceneTree
## End-to-end test of RmlDiagnostics: broken buffer -> painted lines + label.

# Load by path — class_name cache may be stale until next editor import.
const DiagScript := preload("res://addons/rmlui-godot/editor/rml_diagnostics.gd")

var _diag: Node
var _ce: CodeEdit
var _phase := 0


func _initialize() -> void:
	_diag = DiagScript.new()
	root.add_child(_diag)

	var box := VBoxContainer.new()
	root.add_child(box)
	_ce = CodeEdit.new()
	box.add_child(_ce)

	_ce.text = "div {\n\tcolor: red;\n\tbackground-color: nope;;\n\twidth: ;\n}\n"
	_diag.attach(_ce, "rcss")
	create_timer(1.5).timeout.connect(_check)


func _check() -> void:
	match _phase:
		0:
			var painted: PackedInt32Array = _diag._painted_lines
			var label: PanelContainer = _diag._error_bar
			print("rcss painted lines: ", painted)
			print("label visible: ", label.visible if label else "no bar")
			print("label text: ", _diag._error_label.text if _diag._error_label else "-")
			var ok := painted.size() > 0 and label and label.visible
			print("PHASE0 (broken rcss): ", "PASS" if ok else "FAIL")
			if not ok:
				quit(1)
				return
			# Phase 1: fix the buffer -> everything clears.
			_phase = 1
			_ce.text = "div {\n\tcolor: red;\n}\n"
			_ce.text_changed.emit()
			create_timer(1.0).timeout.connect(_check)
		1:
			var painted: PackedInt32Array = _diag._painted_lines
			var label: PanelContainer = _diag._error_bar
			var ok: bool = painted.is_empty() and label and not label.visible
			print("PHASE1 (fixed rcss): ", "PASS" if ok else "FAIL (painted=%s visible=%s)" % [painted, label.visible if label else "?"])
			if not ok:
				quit(1)
				return
			# Phase 2: broken RML document.
			_phase = 2
			_diag.detach()
			# Includes a relative <link> — must NOT produce a false
			# 'Failed to load style sheet' against the diag:// alias.
			_ce.text = "<rml>\n<head>\n<link type=\"text/rcss\" href=\"hello.rcss\"/>\n</head>\n<body>\n<div style=\"width: nope;\">x</div>\n</body>\n</rml>\n"
			_diag.attach(_ce, "rml")
			create_timer(1.0).timeout.connect(_check)
		2:
			var painted: PackedInt32Array = _diag._painted_lines
			var label: PanelContainer = _diag._error_bar
			print("rml painted lines: ", painted)
			print("label text: ", _diag._error_label.text if _diag._error_label else "-")
			var text: String = _diag._error_label.text if _diag._error_label else ""
			var no_false_positive := not text.containsn("style sheet")
			var ok: bool = label and label.visible and no_false_positive
			print("PHASE2 (broken rml, no link false-positive): ", "PASS" if ok else "FAIL")
			quit(0 if ok else 1)
