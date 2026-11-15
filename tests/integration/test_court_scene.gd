extends GutTest
## The generated court scene matches CourtSpec (GDD section 2). Checked with
## physics raycasts, so this tests the colliders themselves, not just node names.

const COURT_SCENE: PackedScene = preload("res://scenes/court/court.tscn")
const EPS: float = 0.01

var court: Node3D


func before_each() -> void:
	court = COURT_SCENE.instantiate()
	add_child_autofree(court)
	# Let the physics server register the bodies.
	await wait_physics_frames(2)


func _ray(from: Vector3, to: Vector3) -> Dictionary:
	var space: PhysicsDirectSpaceState3D = court.get_world_3d().direct_space_state
	var query := PhysicsRayQueryParameters3D.create(from, to, PhysicsLayers.court_mask())
	return space.intersect_ray(query)


func _surface(hit: Dictionary) -> StringName:
	if hit.is_empty():
		return &""
	var collider: Object = hit["collider"]
	return collider.get_meta(&"surface", &"")


func _bodies(node: Node, out: Array[StaticBody3D]) -> Array[StaticBody3D]:
	for child: Node in node.get_children():
		if child is StaticBody3D:
			out.append(child)
		_bodies(child, out)
	return out


func test_every_collider_is_tagged_with_a_surface_zone_and_one_layer() -> void:
	var bodies: Array[StaticBody3D] = _bodies(court, [])
	assert_gt(bodies.size(), 20)
	for body: StaticBody3D in bodies:
		var surface: StringName = body.get_meta(&"surface", &"")
		assert_has(CourtSpec.SURFACES, surface, "%s surface" % body.name)
		assert_true(body.has_meta(&"zone"), "%s zone" % body.name)
		var layer: int = body.collision_layer
		assert_true(layer != 0 and (layer & (layer - 1)) == 0, "%s is on exactly one layer" % body.name)
		assert_eq(body.collision_mask, 0, "%s mask" % body.name)


func test_ground_is_at_y0_across_the_court() -> void:
	for p: Vector2 in [Vector2(0, 3), Vector2(4.9, 9.9), Vector2(-4.9, -9.9), Vector2(2, -6)]:
		var hit: Dictionary = _ray(Vector3(p.x, 2.0, p.y), Vector3(p.x, -1.0, p.y))
		assert_eq(_surface(hit), CourtSpec.SURFACE_GROUND, "ground at %s" % p)
		if not hit.is_empty():
			assert_almost_eq((hit["position"] as Vector3).y, 0.0, EPS)
			assert_eq((hit["collider"] as Node).get_meta(&"zone"), &"court")


func test_runoff_floor_surrounds_the_court() -> void:
	for p: Vector2 in [Vector2(7, 0), Vector2(-7, 5), Vector2(0, 11), Vector2(8, -11.5)]:
		var hit: Dictionary = _ray(Vector3(p.x, 2.0, p.y), Vector3(p.x, -1.0, p.y))
		assert_false(hit.is_empty(), "runoff at %s" % p)
		if not hit.is_empty():
			assert_almost_eq((hit["position"] as Vector3).y, 0.0, EPS)
			assert_eq((hit["collider"] as Node).get_meta(&"zone"), &"runoff")


func test_back_walls_are_3m_glass_then_mesh_to_4m() -> void:
	for half: float in [1.0, -1.0]:
		var glass: Dictionary = _ray(Vector3(0, 1.5, 0), Vector3(0, 1.5, half * 20.0))
		assert_eq(_surface(glass), CourtSpec.SURFACE_GLASS)
		if not glass.is_empty():
			assert_almost_eq((glass["position"] as Vector3).z, half * 10.0, EPS)
		assert_eq(_surface(_ray(Vector3(3, 2.95, 0), Vector3(3, 2.95, half * 20.0))), CourtSpec.SURFACE_GLASS)
		assert_eq(_surface(_ray(Vector3(-3, 3.05, 0), Vector3(-3, 3.05, half * 20.0))), CourtSpec.SURFACE_MESH)
		assert_eq(_surface(_ray(Vector3(0, 3.95, 0), Vector3(0, 3.95, half * 20.0))), CourtSpec.SURFACE_MESH)
		assert_eq(_surface(_ray(Vector3(0, 4.1, 0), Vector3(0, 4.1, half * 20.0))), &"", "open above 4 m")


## Casts from the centre line out through the side wall at (y, z) on both sides.
func _side(y: float, z: float) -> Array[StringName]:
	var out: Array[StringName] = []
	for side: float in [1.0, -1.0]:
		var hit: Dictionary = _ray(Vector3(0, y, z), Vector3(side * 20.0, y, z))
		out.append(_surface(hit))
		if not hit.is_empty():
			assert_almost_eq((hit["position"] as Vector3).x, side * 5.0, EPS, "side wall face at y=%s z=%s" % [y, z])
	return out


func test_side_walls_follow_the_regulation_layout() -> void:
	var glass: StringName = CourtSpec.SURFACE_GLASS
	var mesh: StringName = CourtSpec.SURFACE_MESH
	for half: float in [1.0, -1.0]:
		# First 2 m from the back wall: glass to 3 m, mesh to 4 m.
		assert_eq(_side(2.9, half * 9.0), [glass, glass])
		assert_eq(_side(3.5, half * 9.0), [mesh, mesh])
		assert_eq(_side(4.1, half * 9.0), [&"", &""])
		# Next 2 m, the step: glass to 2 m, mesh to 3 m.
		assert_eq(_side(1.9, half * 7.0), [glass, glass])
		assert_eq(_side(2.5, half * 7.0), [mesh, mesh])
		assert_eq(_side(3.1, half * 7.0), [&"", &""])
		# The exit at the net end of the side glass, open to full height.
		assert_eq(_side(1.0, half * 5.0), [&"", &""])
		assert_eq(_side(2.5, half * 5.0), [&"", &""])
		# Mesh from the exit to the net, 3 m high.
		assert_eq(_side(1.0, half * 2.0), [mesh, mesh])
		assert_eq(_side(2.9, half * 0.5), [mesh, mesh])
		assert_eq(_side(3.1, half * 2.0), [&"", &""])


func test_net_is_088_in_the_middle_and_092_at_the_posts() -> void:
	var net: StringName = CourtSpec.SURFACE_NET
	assert_eq(_surface(_ray(Vector3(0, 0.86, 3), Vector3(0, 0.86, -3))), net, "below the tape at the centre")
	assert_eq(_surface(_ray(Vector3(0.1, 0.90, 3), Vector3(0.1, 0.90, -3))), &"", "over the tape at the centre")
	assert_eq(_surface(_ray(Vector3(4.6, 0.90, 3), Vector3(4.6, 0.90, -3))), net, "below the tape near the post")
	assert_eq(_surface(_ray(Vector3(4.6, 0.95, 3), Vector3(4.6, 0.95, -3))), &"", "over the tape near the post")
	var hit: Dictionary = _ray(Vector3(-2, 0.3, 3), Vector3(-2, 0.3, -3))
	assert_eq(_surface(hit), net)
	if not hit.is_empty():
		assert_almost_eq((hit["position"] as Vector3).z, 0.0, 0.05)


func test_net_segment_tops_follow_the_sag() -> void:
	var net_body: Node = court.get_node("Net")
	var count: int = 0
	for child: Node in net_body.get_children():
		if child is CollisionShape3D and child.name.begins_with("Segment"):
			var shape := child as CollisionShape3D
			var box := shape.shape as BoxShape3D
			var top: float = shape.position.y + box.size.y / 2.0
			assert_almost_eq(top, CourtSpec.net_height_at(shape.position.x), 0.0001)
			count += 1
	assert_eq(count, 20)


func test_ceiling_is_at_8m() -> void:
	var hit: Dictionary = _ray(Vector3(1, 1, 1), Vector3(1, 20, 1))
	assert_eq(_surface(hit), CourtSpec.SURFACE_CEILING)
	if not hit.is_empty():
		assert_almost_eq((hit["position"] as Vector3).y, 8.0, EPS)


func test_service_lines_are_painted_at_695() -> void:
	var line_a: Node3D = court.get_node("Lines/ServiceLineA")
	var line_b: Node3D = court.get_node("Lines/ServiceLineB")
	assert_almost_eq(line_a.position.z, 6.95, 0.0001)
	assert_almost_eq(line_b.position.z, -6.95, 0.0001)
