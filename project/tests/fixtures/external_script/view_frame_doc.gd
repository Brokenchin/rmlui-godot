extends RefCounted
## Fixture script loaded via <script src="…"> for test_external_script.gd.
## Mirrors the issue-25 repro: a GDScript sitting next to the .rml that the
## document pulls in by relative (and absolute) src.

var rml_context
var count := 0

func _on_load(event):
	count += 1
	if rml_context != null:
		rml_context.set_meta("external_script_ran", count)
		rml_context.set_meta("external_event_type", str(event.get("type", "")))

func render() -> String:
	return "external-ok"
