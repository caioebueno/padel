extends GutTest
## The input map (GDD section 7) and physics layer names in project.godot
## match the constants code uses.


func test_every_action_exists_with_a_key_and_a_gamepad_binding() -> void:
	for action: StringName in InputActions.ALL:
		assert_true(InputMap.has_action(action), "missing action %s" % action)
		if not InputMap.has_action(action):
			continue
		var has_key: bool = false
		var has_pad: bool = false
		for event: InputEvent in InputMap.action_get_events(action):
			if event is InputEventKey:
				has_key = true
			elif event is InputEventJoypadButton or event is InputEventJoypadMotion:
				has_pad = true
		assert_true(has_key, "%s has no keyboard binding" % action)
		assert_true(has_pad, "%s has no gamepad binding" % action)


func _first_key(action: StringName) -> Key:
	for event: InputEvent in InputMap.action_get_events(action):
		if event is InputEventKey:
			return (event as InputEventKey).physical_keycode
	return KEY_NONE


func _first_button(action: StringName) -> JoyButton:
	for event: InputEvent in InputMap.action_get_events(action):
		if event is InputEventJoypadButton:
			return (event as InputEventJoypadButton).button_index
	return JOY_BUTTON_INVALID


func test_bindings_match_the_gdd_controls_table() -> void:
	assert_eq(_first_key(InputActions.MOVE_FORWARD), KEY_W)
	assert_eq(_first_key(InputActions.MOVE_LEFT), KEY_A)
	assert_eq(_first_key(InputActions.SPRINT), KEY_SHIFT)
	assert_eq(_first_key(InputActions.SHOT_DRIVE), KEY_J)
	assert_eq(_first_key(InputActions.SHOT_LOB), KEY_K)
	assert_eq(_first_key(InputActions.SHOT_OVERHEAD), KEY_L)
	assert_eq(_first_key(InputActions.SHOT_DROP), KEY_I)
	assert_eq(_first_key(InputActions.MOD_ATTACK), KEY_E)
	assert_eq(_first_key(InputActions.MOD_TOUCH), KEY_Q)
	assert_eq(_first_key(InputActions.CMD_NET), KEY_1)
	assert_eq(_first_key(InputActions.CMD_YOURS), KEY_4)
	assert_eq(_first_key(InputActions.PAUSE), KEY_ESCAPE)
	assert_eq(_first_button(InputActions.SHOT_DRIVE), JOY_BUTTON_A)
	assert_eq(_first_button(InputActions.SHOT_LOB), JOY_BUTTON_B)
	assert_eq(_first_button(InputActions.SHOT_OVERHEAD), JOY_BUTTON_Y)
	assert_eq(_first_button(InputActions.SHOT_DROP), JOY_BUTTON_X)
	assert_eq(_first_button(InputActions.MOD_ATTACK), JOY_BUTTON_RIGHT_SHOULDER)
	assert_eq(_first_button(InputActions.MOD_TOUCH), JOY_BUTTON_LEFT_SHOULDER)
	assert_eq(_first_button(InputActions.CMD_NET), JOY_BUTTON_DPAD_UP)
	assert_eq(_first_button(InputActions.CMD_BACK), JOY_BUTTON_DPAD_DOWN)
	assert_eq(_first_button(InputActions.CMD_MIDDLE), JOY_BUTTON_DPAD_LEFT)
	assert_eq(_first_button(InputActions.CMD_YOURS), JOY_BUTTON_DPAD_RIGHT)
	assert_eq(_first_button(InputActions.PAUSE), JOY_BUTTON_START)


func test_stick_up_means_forward() -> void:
	var found: bool = false
	for event: InputEvent in InputMap.action_get_events(InputActions.MOVE_FORWARD):
		if event is InputEventJoypadMotion:
			var motion := event as InputEventJoypadMotion
			assert_eq(motion.axis, JOY_AXIS_LEFT_Y)
			assert_lt(motion.axis_value, 0.0, "Godot reports stick-up as negative Y")
			found = true
	assert_true(found)


func test_physics_layer_names_match_physics_layers() -> void:
	for layer: int in PhysicsLayers.LAYER_NAMES:
		var setting: String = "layer_names/3d_physics/layer_%d" % layer
		assert_eq(ProjectSettings.get_setting(setting, ""), PhysicsLayers.LAYER_NAMES[layer], setting)


func test_layer_bits() -> void:
	assert_eq(PhysicsLayers.bit(1), 1)
	assert_eq(PhysicsLayers.bit(PhysicsLayers.GLASS), 2)
	assert_eq(PhysicsLayers.court_mask() & PhysicsLayers.bit(PhysicsLayers.PLAYERS), 0)


func test_main_scene_is_set() -> void:
	var main_scene: String = ProjectSettings.get_setting("application/run/main_scene", "")
	assert_eq(main_scene, "res://scenes/match/main.tscn")
	assert_true(ResourceLoader.exists(main_scene))
