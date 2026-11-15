extends GutTest
## Static checks for the CLAUDE.md hard rules on scripts/engine/:
## headless (no Nodes), deterministic (no global RNG, no wall clock), and the
## ball is not a RigidBody. Breaking any of these breaks tests, replays, the
## soak run and future online play, so QA gates it in CI.

const ENGINE_DIR: String = "res://scripts/engine"

## Pattern -> why it is banned. Matched on code with comments stripped.
const BANNED: Dictionary[String, String] = {
	"(?<![.\\w])(randf|randi|randf_range|randi_range|randfn|randomize|seed)\\s*\\(":
		"global RNG; take a seeded RandomNumberGenerator as a parameter",
	"\\bTime\\.get_(ticks|unix|datetime|time)":
		"wall clock; engine code runs at a fixed step",
	"\\bOS\\.get_ticks":
		"wall clock; engine code runs at a fixed step",
	"\\bget_tree\\s*\\(":
		"scene tree; engine code is headless",
	"\\bRigidBody3D\\b":
		"the ball uses our own integrator, not RigidBody",
	"\\bget_process_delta_time|get_physics_process_delta_time":
		"frame time; pass the fixed step in",
}


func _engine_files(dir_path: String = ENGINE_DIR) -> Array[String]:
	var files: Array[String] = []
	var dir: DirAccess = DirAccess.open(dir_path)
	if dir == null:
		return files
	for sub: String in dir.get_directories():
		files.append_array(_engine_files(dir_path.path_join(sub)))
	for file: String in dir.get_files():
		if file.ends_with(".gd"):
			files.append(dir_path.path_join(file))
	return files


func _code_without_comments(path: String) -> String:
	var out: PackedStringArray = []
	var strip: RegEx = RegEx.create_from_string("#.*$")
	for line: String in FileAccess.get_file_as_string(path).split("\n"):
		out.append(strip.sub(line, ""))
	return "\n".join(out)


func test_engine_folder_has_scripts() -> void:
	assert_gt(_engine_files().size(), 0)


func test_engine_code_avoids_banned_apis() -> void:
	for path: String in _engine_files():
		var code: String = _code_without_comments(path)
		for pattern: String in BANNED:
			var m: RegExMatch = RegEx.create_from_string(pattern).search(code)
			assert_null(m, "%s uses '%s': %s" % [
				path, m.get_string() if m else "", BANNED[pattern]])


func test_engine_classes_are_not_nodes() -> void:
	for path: String in _engine_files():
		var script: Script = load(path) as Script
		assert_not_null(script, "%s failed to load" % path)
		if script == null:
			continue
		var base: StringName = script.get_instance_base_type()
		assert_false(ClassDB.is_parent_class(base, &"Node") or base == &"Node",
			"%s is a %s; engine code must be RefCounted or Resource" % [path, base])
