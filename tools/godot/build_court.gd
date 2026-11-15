extends SceneTree
## Generates the placeholder court scene and the main scene from CourtSpec:
##
##   godot --headless --path . --script res://tools/godot/build_court.gd
##
## Outputs (commit them; CI checks they are up to date):
##   scenes/court/court.tscn        primitive court with collision on every surface
##   scenes/match/main.tscn         court + broadcast camera + lights, the project's main scene
##   assets/materials/placeholder/  one material per surface, for Agent 5 to replace
##
## Every collider is a StaticBody3D carrying metadata "surface" (CourtSpec.SURFACE_*)
## and "zone" ("court", "runoff" or "ceiling"), on the layer from PhysicsLayers.

const Spec: GDScript = preload("res://scripts/engine/court_spec.gd")
const Layers: GDScript = preload("res://scripts/engine/physics_layers.gd")

const COURT_SCENE_PATH: String = "res://scenes/court/court.tscn"
const MAIN_SCENE_PATH: String = "res://scenes/match/main.tscn"
const MATERIAL_DIR: String = "res://assets/materials/placeholder/"

## Collision segments across the net, so the box tops follow the sagging tape.
const NET_SEGMENTS: int = 20
const NET_THICKNESS: float = 0.04
const NET_POST_SIZE: float = 0.08
const LINE_LIFT: float = 0.002

# Placeholder camera, GDD 9: 7-9 m high, 6-8 m behind the Team A back glass.
# Presentation (Agent 7) moves these values into data/camera/.
const CAMERA_HEIGHT: float = 8.0
const CAMERA_BEHIND_GLASS: float = 7.0
const CAMERA_FOV: float = 50.0

var materials: Dictionary[StringName, Material] = {}
var court_root: Node3D


func _init() -> void:
	var ok: bool = _build_materials() and _build_court() and _build_main()
	print("build_court: %s" % ("done" if ok else "FAILED"))
	quit(0 if ok else 1)


# --- Materials ---------------------------------------------------------------

func _build_materials() -> bool:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(MATERIAL_DIR))
	var defs: Dictionary = {
		&"court_turf": {"color": Color("1e4c9a"), "roughness": 0.95},
		&"runoff": {"color": Color("2a2f38"), "roughness": 0.95},
		&"line": {"color": Color("f4f6f8"), "roughness": 0.8},
		&"glass": {"color": Color(0.75, 0.88, 1.0, 0.16), "roughness": 0.05, "metallic": 0.2, "alpha": true},
		&"mesh_fence": {"color": Color(0.12, 0.13, 0.15, 0.55), "roughness": 0.6, "metallic": 0.6, "alpha": true},
		&"net": {"color": Color(0.08, 0.08, 0.1, 0.7), "roughness": 0.9, "alpha": true, "double_sided": true},
		&"net_tape": {"color": Color("f4f6f8"), "roughness": 0.8, "double_sided": true},
		&"metal_frame": {"color": Color("23262b"), "roughness": 0.4, "metallic": 0.8},
	}
	for mat_name: StringName in defs:
		var d: Dictionary = defs[mat_name]
		var mat := StandardMaterial3D.new()
		mat.resource_name = "placeholder_%s" % mat_name
		mat.albedo_color = d["color"]
		mat.roughness = d.get("roughness", 1.0)
		mat.metallic = d.get("metallic", 0.0)
		if d.get("alpha", false):
			mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		if d.get("double_sided", false):
			mat.cull_mode = BaseMaterial3D.CULL_DISABLED
		var path: String = MATERIAL_DIR + "%s.tres" % mat_name
		var err: Error = ResourceSaver.save(mat, path)
		if err != OK:
			push_error("Could not save %s: %s" % [path, error_string(err)])
			return false
		materials[mat_name] = load(path)
	return true


# --- Court -------------------------------------------------------------------

func _build_court() -> bool:
	court_root = Node3D.new()
	court_root.name = "Court"
	court_root.set_meta(&"generated_by", "tools/godot/build_court.gd")

	_build_floor()
	_build_lines()
	_build_walls()
	_build_net()
	_build_ceiling()
	return _save_scene(court_root, COURT_SCENE_PATH)


func _build_floor() -> void:
	var floor_group: Node3D = _group(court_root, "Floor")
	var t: float = Spec.GROUND_THICKNESS
	_box_body(floor_group, "Ground", Vector3(0.0, -t / 2.0, 0.0),
		Vector3(Spec.WIDTH, t, Spec.LENGTH),
		Spec.SURFACE_GROUND, &"court", Layers.GROUND, &"court_turf")

	# Run-off: a frame of floor around the court, under the walls and beyond,
	# so players and the ball can leave through the exits (out-of-court play).
	var outer_l: float = Spec.LENGTH + 2.0 * Spec.RUNOFF_BACK
	var side_x: float = Spec.HALF_WIDTH + Spec.RUNOFF_SIDE / 2.0
	var back_z: float = Spec.HALF_LENGTH + Spec.RUNOFF_BACK / 2.0
	var runoff: StaticBody3D = _body(floor_group, "Runoff", Spec.SURFACE_GROUND, &"runoff", Layers.RUNOFF)
	for side: float in [-1.0, 1.0]:
		var label: String = "Pos" if side > 0.0 else "Neg"
		_box_part(runoff, "SideX%s" % label, Vector3(side * side_x, -t / 2.0, 0.0),
			Vector3(Spec.RUNOFF_SIDE, t, outer_l), &"runoff")
		_box_part(runoff, "BackZ%s" % label, Vector3(0.0, -t / 2.0, side * back_z),
			Vector3(Spec.WIDTH, t, Spec.RUNOFF_BACK), &"runoff")


func _build_lines() -> void:
	var lines: Node3D = _group(court_root, "Lines")
	var w: float = Spec.LINE_WIDTH
	var y: float = LINE_LIFT
	for half: float in [1.0, -1.0]:
		var label: String = "A" if half > 0.0 else "B"
		_visual_box(lines, "ServiceLine%s" % label, Vector3(0.0, y, half * Spec.SERVICE_LINE_Z),
			Vector3(Spec.WIDTH, 0.002, w), &"line")
		var centre_len: float = Spec.SERVICE_LINE_Z + Spec.CENTRE_LINE_EXTENSION
		_visual_box(lines, "CentreLine%s" % label, Vector3(0.0, y, half * centre_len / 2.0),
			Vector3(w, 0.002, centre_len), &"line")


func _build_walls() -> void:
	var walls: Node3D = _group(court_root, "Walls")
	var t: float = Spec.WALL_THICKNESS

	for half: float in [1.0, -1.0]:
		var label: String = "A" if half > 0.0 else "B"
		var z: float = half * (Spec.HALF_LENGTH + t / 2.0)
		# Back wall spans the corners too, so there is no gap where it meets the sides.
		var back_w: float = Spec.WIDTH + 2.0 * t
		_box_body(walls, "BackGlass%s" % label,
			Vector3(0.0, Spec.BACK_GLASS_HEIGHT / 2.0, z),
			Vector3(back_w, Spec.BACK_GLASS_HEIGHT, t),
			Spec.SURFACE_GLASS, &"court", Layers.GLASS, &"glass")
		var fence_h: float = Spec.FENCE_TOTAL_HEIGHT - Spec.BACK_GLASS_HEIGHT
		_box_body(walls, "BackFence%s" % label,
			Vector3(0.0, Spec.BACK_GLASS_HEIGHT + fence_h / 2.0, z),
			Vector3(back_w, fence_h, t),
			Spec.SURFACE_MESH, &"court", Layers.MESH, &"mesh_fence")

		for side: float in [1.0, -1.0]:
			var side_label: String = "%s_X%s" % [label, "Pos" if side > 0.0 else "Neg"]
			var x: float = side * (Spec.HALF_WIDTH + t / 2.0)
			_side_panel(walls, "SideGlassFull%s" % side_label, x, half,
				Spec.SIDE_GLASS_STEP_START_Z, Spec.HALF_LENGTH, 0.0, Spec.SIDE_GLASS_FULL_HEIGHT,
				Spec.SURFACE_GLASS, Layers.GLASS, &"glass")
			_side_panel(walls, "SideFenceAboveFull%s" % side_label, x, half,
				Spec.SIDE_GLASS_STEP_START_Z, Spec.HALF_LENGTH, Spec.SIDE_GLASS_FULL_HEIGHT, Spec.FENCE_TOTAL_HEIGHT,
				Spec.SURFACE_MESH, Layers.MESH, &"mesh_fence")
			_side_panel(walls, "SideGlassStep%s" % side_label, x, half,
				Spec.SIDE_GLASS_END_Z, Spec.SIDE_GLASS_STEP_START_Z, 0.0, Spec.SIDE_GLASS_STEP_HEIGHT,
				Spec.SURFACE_GLASS, Layers.GLASS, &"glass")
			_side_panel(walls, "SideFenceAboveStep%s" % side_label, x, half,
				Spec.SIDE_GLASS_END_Z, Spec.SIDE_GLASS_STEP_START_Z, Spec.SIDE_GLASS_STEP_HEIGHT, Spec.SIDE_MESH_HEIGHT,
				Spec.SURFACE_MESH, Layers.MESH, &"mesh_fence")
			# The exit sits between EXIT_NEAR_Z and EXIT_FAR_Z, open to full height.
			_side_panel(walls, "SideFence%s" % side_label, x, half,
				0.0, Spec.EXIT_NEAR_Z, 0.0, Spec.SIDE_MESH_HEIGHT,
				Spec.SURFACE_MESH, Layers.MESH, &"mesh_fence")


## A side wall panel between |z| = z_from and z_to on the given half, from y_from to y_to.
func _side_panel(parent: Node3D, panel_name: String, x: float, half: float,
		z_from: float, z_to: float, y_from: float, y_to: float,
		surface: StringName, layer: int, material: StringName) -> void:
	var length: float = z_to - z_from
	var height: float = y_to - y_from
	_box_body(parent, panel_name,
		Vector3(x, y_from + height / 2.0, half * (z_from + length / 2.0)),
		Vector3(Spec.WALL_THICKNESS, height, length),
		surface, &"court", layer, material)


func _build_net() -> void:
	var net: StaticBody3D = _body(court_root, "Net", Spec.SURFACE_NET, &"court", Layers.NET)
	var seg_w: float = Spec.WIDTH / NET_SEGMENTS
	for i: int in NET_SEGMENTS:
		var cx: float = -Spec.HALF_WIDTH + seg_w * (i + 0.5)
		var h: float = Spec.net_height_at(cx)
		var shape := CollisionShape3D.new()
		shape.name = "Segment%02d" % i
		var box := BoxShape3D.new()
		box.size = Vector3(seg_w, h, NET_THICKNESS)
		shape.shape = box
		shape.position = Vector3(cx, h / 2.0, 0.0)
		net.add_child(shape)

	var visual := MeshInstance3D.new()
	visual.name = "NetMesh"
	visual.mesh = _net_mesh()
	net.add_child(visual)

	var tape := MeshInstance3D.new()
	tape.name = "TapeMesh"
	tape.mesh = _net_tape_mesh()
	net.add_child(tape)

	for side: float in [-1.0, 1.0]:
		# Outer face of each post lines up with the side limit of the court.
		var px: float = side * (Spec.HALF_WIDTH - NET_POST_SIZE / 2.0)
		_box_part(net, "Post%s" % ("XPos" if side > 0.0 else "XNeg"),
			Vector3(px, Spec.NET_POST_HEIGHT / 2.0, 0.0),
			Vector3(NET_POST_SIZE, Spec.NET_POST_HEIGHT, NET_POST_SIZE), &"metal_frame")


func _net_mesh() -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	st.set_material(materials[&"net"])
	var steps: int = 40
	for i: int in steps:
		var x0: float = -Spec.HALF_WIDTH + Spec.WIDTH * i / steps
		var x1: float = -Spec.HALF_WIDTH + Spec.WIDTH * (i + 1) / steps
		var top0: float = Spec.net_height_at(x0)
		var top1: float = Spec.net_height_at(x1)
		st.set_normal(Vector3.BACK)
		st.add_vertex(Vector3(x0, 0.0, 0.0))
		st.add_vertex(Vector3(x1, 0.0, 0.0))
		st.add_vertex(Vector3(x1, top1, 0.0))
		st.add_vertex(Vector3(x0, 0.0, 0.0))
		st.add_vertex(Vector3(x1, top1, 0.0))
		st.add_vertex(Vector3(x0, top0, 0.0))
	return st.commit()


func _net_tape_mesh() -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	st.set_material(materials[&"net_tape"])
	var steps: int = 40
	var band: float = 0.05
	for i: int in steps:
		var x0: float = -Spec.HALF_WIDTH + Spec.WIDTH * i / steps
		var x1: float = -Spec.HALF_WIDTH + Spec.WIDTH * (i + 1) / steps
		var top0: float = Spec.net_height_at(x0)
		var top1: float = Spec.net_height_at(x1)
		st.set_normal(Vector3.BACK)
		st.add_vertex(Vector3(x0, top0 - band, 0.001))
		st.add_vertex(Vector3(x1, top1 - band, 0.001))
		st.add_vertex(Vector3(x1, top1, 0.001))
		st.add_vertex(Vector3(x0, top0 - band, 0.001))
		st.add_vertex(Vector3(x1, top1, 0.001))
		st.add_vertex(Vector3(x0, top0, 0.001))
	return st.commit()


func _build_ceiling() -> void:
	# Invisible: it only exists so a raycast or a debug tool can find it.
	var t: float = Spec.GROUND_THICKNESS
	var ceiling: StaticBody3D = _body(court_root, "Ceiling", Spec.SURFACE_CEILING, &"ceiling", Layers.CEILING)
	var shape := CollisionShape3D.new()
	shape.name = "Shape"
	var box := BoxShape3D.new()
	box.size = Vector3(Spec.WIDTH + 2.0 * Spec.RUNOFF_SIDE, t, Spec.LENGTH + 2.0 * Spec.RUNOFF_BACK)
	shape.shape = box
	shape.position = Vector3(0.0, Spec.CEILING_HEIGHT + t / 2.0, 0.0)
	ceiling.add_child(shape)


# --- Main scene --------------------------------------------------------------

func _build_main() -> bool:
	var main := Node3D.new()
	main.name = "Main"

	var court_scene: PackedScene = load(COURT_SCENE_PATH)
	var court: Node = court_scene.instantiate()
	main.add_child(court)

	var env := WorldEnvironment.new()
	env.name = "WorldEnvironment"
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color("101418")
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color("fff1dc")
	environment.ambient_light_energy = 0.25
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.environment = environment
	main.add_child(env)

	# Warm overhead lights (GDD 10), as one key light plus fill.
	var key := DirectionalLight3D.new()
	key.name = "KeyLight"
	key.light_color = Color("fff3e0")
	key.light_energy = 0.8
	key.shadow_enabled = true
	key.rotation_degrees = Vector3(-60.0, 25.0, 0.0)
	main.add_child(key)

	var camera := Camera3D.new()
	camera.name = "BroadcastCamera"
	camera.fov = CAMERA_FOV
	camera.current = true
	var cam_pos := Vector3(0.0, CAMERA_HEIGHT, Spec.HALF_LENGTH + CAMERA_BEHIND_GLASS)
	camera.transform = Transform3D(Basis.looking_at(Vector3(0.0, 0.0, -2.0) - cam_pos, Vector3.UP), cam_pos)
	main.add_child(camera)

	return _save_scene(main, MAIN_SCENE_PATH)


# --- Helpers -----------------------------------------------------------------

func _group(parent: Node, group_name: String) -> Node3D:
	var node := Node3D.new()
	node.name = group_name
	parent.add_child(node)
	return node


func _body(parent: Node, body_name: String, surface: StringName, zone: StringName, layer: int) -> StaticBody3D:
	var body := StaticBody3D.new()
	body.name = body_name
	body.collision_layer = Layers.bit(layer)
	# Static geometry never needs to detect anything itself.
	body.collision_mask = 0
	body.set_meta(&"surface", surface)
	body.set_meta(&"zone", zone)
	parent.add_child(body)
	return body


## One StaticBody3D holding a single box collider and its matching mesh.
func _box_body(parent: Node, body_name: String, centre: Vector3, size: Vector3,
		surface: StringName, zone: StringName, layer: int, material: StringName) -> StaticBody3D:
	var body: StaticBody3D = _body(parent, body_name, surface, zone, layer)
	body.position = centre
	_box_part(body, "Box", Vector3.ZERO, size, material)
	return body


## A box collider plus its mesh, added to an existing body.
func _box_part(body: Node3D, part_name: String, centre: Vector3, size: Vector3, material: StringName) -> void:
	var shape := CollisionShape3D.new()
	shape.name = "%sShape" % part_name
	var box := BoxShape3D.new()
	box.size = size
	shape.shape = box
	shape.position = centre
	body.add_child(shape)
	_visual_box(body, "%sMesh" % part_name, centre, size, material)


func _visual_box(parent: Node, mesh_name: String, centre: Vector3, size: Vector3, material: StringName) -> void:
	var mi := MeshInstance3D.new()
	mi.name = mesh_name
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = materials[material]
	mi.mesh = mesh
	mi.position = centre
	parent.add_child(mi)


func _save_scene(scene_root: Node, path: String) -> bool:
	_set_owner_recursive(scene_root, scene_root)
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(path.get_base_dir()))
	var packed := PackedScene.new()
	var err: Error = packed.pack(scene_root)
	if err == OK:
		err = ResourceSaver.save(packed, path)
	if err != OK:
		push_error("Could not save %s: %s" % [path, error_string(err)])
	scene_root.free()
	return err == OK


func _set_owner_recursive(node: Node, scene_owner: Node) -> void:
	for child: Node in node.get_children():
		# Children of an instanced scene belong to that scene, not to us.
		if child.owner == null:
			child.owner = scene_owner
		if child.scene_file_path.is_empty():
			_set_owner_recursive(child, scene_owner)
