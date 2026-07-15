extends SceneTree
## Headless-windowed screenshot helper: loads a scene, lets it settle, grabs the
## root viewport, saves a PNG, quits. Usage:
##   Godot --path . -s tests/visual/_shot.gd -- <res://scene.tscn> <res://out.png>
var _out := "res://issue61_shot.png"


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var scene_path := "res://tests/visual/issue61_clip_demo.tscn"
	if args.size() >= 1:
		scene_path = args[0]
	if args.size() >= 2:
		_out = args[1]
	var scene = load(scene_path).instantiate()
	root.add_child(scene)
	create_timer(1.2).timeout.connect(_grab)


func _grab() -> void:
	RenderingServer.frame_post_draw.connect(_save, CONNECT_ONE_SHOT)


func _save() -> void:
	var img := root.get_viewport().get_texture().get_image()
	var err := img.save_png(_out)
	print("[shot] saved %s err=%s size=%s" % [_out, err, img.get_size()])
	quit(0)
