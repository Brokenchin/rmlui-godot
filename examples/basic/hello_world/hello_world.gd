extends Control

# Hello World is configured entirely from the inspector — see the RmlContext
# node's Auto-Configuration group:
#   document_path = res://addons/rmlui-godot/examples/basic/hello_world/hello.rml
#   font_paths    = [NotoSans-Regular.ttf, NotoSans-Bold.ttf]
#
# Property-based setup also renders in the editor (2D viewport and the
# RmlUI Preview bottom panel). Script-driven loading does NOT run in the
# editor, so prefer the properties unless you need runtime logic.
#
# The equivalent setup in code would be:
#   var rml: RmlContext = $RmlContext
#   rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Regular.ttf")
#   rml.load_font_face("res://addons/rmlui-godot/examples/fonts/NotoSans-Bold.ttf")
#   rml.load_document("res://addons/rmlui-godot/examples/basic/hello_world/hello.rml")
