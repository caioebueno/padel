extends GutTest
## The main scene loads and frames the court from the broadcast position (GDD 9).

const MAIN_SCENE: PackedScene = preload("res://scenes/match/main.tscn")


func test_main_scene_instantiates_with_the_court_and_a_camera() -> void:
	var main: Node3D = MAIN_SCENE.instantiate()
	add_child_autofree(main)
	await wait_process_frames(2)
	assert_not_null(main.get_node_or_null("Court"))
	var camera: Camera3D = main.get_node_or_null("BroadcastCamera")
	assert_not_null(camera)
	if camera == null:
		return
	assert_true(camera.current)
	var pos: Vector3 = camera.global_position
	assert_between(pos.y, 7.0, 9.0, "camera height")
	assert_between(pos.z - CourtSpec.HALF_LENGTH, 6.0, 8.0, "behind Team A's glass")
	# Looking down the court toward Team B.
	var forward: Vector3 = -camera.global_transform.basis.z
	assert_lt(forward.z, 0.0)
	assert_lt(forward.y, 0.0)
