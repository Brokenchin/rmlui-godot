extends Control

var _items: Array[Dictionary] = [
	{"name": "Health Potion", "qty": 3},
	{"name": "Iron Sword", "qty": 1},
	{"name": "Torch", "qty": 5},
]

func _ready():
	var ctx: RmlContext = $RmlContext
	ctx.document_path = "res://examples/data_binding/data_binding.rml"

	ctx.create_data_model_from_dict("player", {
		"name": "Adventurer",
		"level": 5,
		"health": 85,
		"max_health": 100,
		"mana": 40,
		"max_mana": 60,
		"xp": 1250,
		"xp_next": 2000,
		"has_items": true,
		"items": _items,
	})

	# Simulate stat changes every 2 seconds
	var timer := Timer.new()
	timer.wait_time = 2.0
	timer.timeout.connect(_on_tick)
	add_child(timer)
	timer.start()

var _tick := 0
func _on_tick():
	_tick += 1
	var ctx: RmlContext = $RmlContext
	var health = 85 - (_tick * 7) % 86
	var mana = 40 + (_tick * 3) % 21
	ctx.update_data_model("player", {
		"health": health,
		"mana": mana,
		"xp": 1250 + _tick * 50,
	})
