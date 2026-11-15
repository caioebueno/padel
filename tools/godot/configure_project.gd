extends SceneTree
## Writes the input map (GDD section 7) and the physics layer names into
## project.godot. Re-run it after changing a binding below:
##
##   godot --headless --path . --script res://tools/godot/configure_project.gd
##
## Keeping bindings here, rather than hand-editing project.godot, keeps the
## serialized InputEvent blocks correct and the table readable.

const InputActionsScript: GDScript = preload("res://scripts/gameplay/input_actions.gd")
const PhysicsLayersScript: GDScript = preload("res://scripts/engine/physics_layers.gd")

const STICK_DEADZONE: float = 0.2
const BUTTON_DEADZONE: float = 0.5

# Xbox layout. Godot's JoyButton values follow the SDL layout, so these map to
# the matching buttons on PlayStation and Switch pads.
# Each entry: action -> {keys: [physical keycodes], buttons: [JoyButton], axes: [[JoyAxis, sign]]}
var bindings: Dictionary = {
	&"move_left": {"keys": [KEY_A, KEY_LEFT], "axes": [[JOY_AXIS_LEFT_X, -1.0]], "deadzone": STICK_DEADZONE},
	&"move_right": {"keys": [KEY_D, KEY_RIGHT], "axes": [[JOY_AXIS_LEFT_X, 1.0]], "deadzone": STICK_DEADZONE},
	&"move_forward": {"keys": [KEY_W, KEY_UP], "axes": [[JOY_AXIS_LEFT_Y, -1.0]], "deadzone": STICK_DEADZONE},
	&"move_back": {"keys": [KEY_S, KEY_DOWN], "axes": [[JOY_AXIS_LEFT_Y, 1.0]], "deadzone": STICK_DEADZONE},
	&"sprint": {"keys": [KEY_SHIFT], "axes": [[JOY_AXIS_TRIGGER_RIGHT, 1.0]]},
	&"shot_drive": {"keys": [KEY_J], "buttons": [JOY_BUTTON_A]},
	&"shot_lob": {"keys": [KEY_K], "buttons": [JOY_BUTTON_B]},
	&"shot_overhead": {"keys": [KEY_L], "buttons": [JOY_BUTTON_Y]},
	&"shot_drop": {"keys": [KEY_I], "buttons": [JOY_BUTTON_X]},
	&"mod_attack": {"keys": [KEY_E], "buttons": [JOY_BUTTON_RIGHT_SHOULDER]},
	&"mod_touch": {"keys": [KEY_Q], "buttons": [JOY_BUTTON_LEFT_SHOULDER]},
	&"cmd_net": {"keys": [KEY_1], "buttons": [JOY_BUTTON_DPAD_UP]},
	&"cmd_back": {"keys": [KEY_2], "buttons": [JOY_BUTTON_DPAD_DOWN]},
	&"cmd_middle": {"keys": [KEY_3], "buttons": [JOY_BUTTON_DPAD_LEFT]},
	&"cmd_yours": {"keys": [KEY_4], "buttons": [JOY_BUTTON_DPAD_RIGHT]},
	&"pause": {"keys": [KEY_ESCAPE], "buttons": [JOY_BUTTON_START]},
}


func _init() -> void:
	var ok: bool = _write_input_map() and _write_layer_names()
	if ok:
		var err: Error = ProjectSettings.save()
		if err != OK:
			push_error("Could not save project.godot: %s" % error_string(err))
			ok = false
	print("configure_project: %s" % ("done" if ok else "FAILED"))
	quit(0 if ok else 1)


func _write_input_map() -> bool:
	var declared: Array = InputActionsScript.ALL
	for action: StringName in declared:
		if not bindings.has(action):
			push_error("InputActions.%s has no binding in configure_project.gd" % action)
			return false
	for action: StringName in bindings:
		if not declared.has(action):
			push_error("Binding %s is not declared in InputActions.ALL" % action)
			return false
		var spec: Dictionary = bindings[action]
		var events: Array[InputEvent] = []
		for keycode: Key in spec.get("keys", []):
			var key := InputEventKey.new()
			key.physical_keycode = keycode
			key.device = -1
			events.append(key)
		for button: JoyButton in spec.get("buttons", []):
			var joy_button := InputEventJoypadButton.new()
			joy_button.button_index = button
			joy_button.device = -1
			events.append(joy_button)
		for axis_spec: Array in spec.get("axes", []):
			var joy_axis := InputEventJoypadMotion.new()
			joy_axis.axis = axis_spec[0] as JoyAxis
			joy_axis.axis_value = axis_spec[1] as float
			joy_axis.device = -1
			events.append(joy_axis)
		var deadzone: float = spec.get("deadzone", BUTTON_DEADZONE)
		ProjectSettings.set_setting("input/%s" % action, {"deadzone": deadzone, "events": events})
	return true


func _write_layer_names() -> bool:
	var names: Dictionary = PhysicsLayersScript.LAYER_NAMES
	for layer: int in names:
		ProjectSettings.set_setting("layer_names/3d_physics/layer_%d" % layer, names[layer])
	return true
