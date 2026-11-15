extends GutTest
## GDD section 2: court measurements and helpers.


func test_playing_area_is_20_by_10() -> void:
	assert_eq(CourtSpec.LENGTH, 20.0)
	assert_eq(CourtSpec.WIDTH, 10.0)
	assert_eq(CourtSpec.HALF_LENGTH, 10.0)
	assert_eq(CourtSpec.HALF_WIDTH, 5.0)


func test_net_is_088_in_the_centre_and_092_at_the_posts() -> void:
	assert_almost_eq(CourtSpec.net_height_at(0.0), 0.88, 0.0001)
	assert_almost_eq(CourtSpec.net_height_at(5.0), 0.92, 0.0001)
	assert_almost_eq(CourtSpec.net_height_at(-5.0), 0.92, 0.0001)


func test_net_height_rises_monotonically_toward_the_posts() -> void:
	var previous: float = CourtSpec.net_height_at(0.0)
	for i: int in range(1, 11):
		var h: float = CourtSpec.net_height_at(i * 0.5)
		assert_gt(h, previous, "net height at x=%.1f" % (i * 0.5))
		previous = h


func test_net_height_is_clamped_outside_the_court() -> void:
	assert_almost_eq(CourtSpec.net_height_at(7.0), 0.92, 0.0001)


func test_service_line_is_695_from_the_net_and_305_from_the_back_wall() -> void:
	assert_eq(CourtSpec.SERVICE_LINE_Z, 6.95)
	assert_almost_eq(CourtSpec.HALF_LENGTH - CourtSpec.SERVICE_LINE_Z, 3.05, 0.0001)


func test_side_glass_has_a_3m_panel_then_a_2m_step() -> void:
	assert_eq(CourtSpec.side_glass_height_at(9.5), 3.0)
	assert_eq(CourtSpec.side_glass_height_at(-9.5), 3.0)
	assert_eq(CourtSpec.side_glass_height_at(7.0), 2.0)
	assert_eq(CourtSpec.side_glass_height_at(-7.0), 2.0)
	assert_eq(CourtSpec.side_glass_height_at(5.0), 0.0)
	assert_eq(CourtSpec.side_glass_height_at(1.0), 0.0)


func test_exit_sits_at_the_net_end_of_the_side_glass() -> void:
	assert_eq(CourtSpec.EXIT_FAR_Z, 6.0)
	assert_eq(CourtSpec.EXIT_NEAR_Z, 4.0)
	assert_true(CourtSpec.is_exit_at(5.0))
	assert_true(CourtSpec.is_exit_at(-5.0))
	assert_false(CourtSpec.is_exit_at(7.0))
	assert_false(CourtSpec.is_exit_at(2.0))


func test_inside_court_includes_the_boundary() -> void:
	assert_true(CourtSpec.is_inside_court(0.0, 0.0))
	assert_true(CourtSpec.is_inside_court(5.0, 10.0))
	assert_false(CourtSpec.is_inside_court(5.01, 0.0))
	assert_false(CourtSpec.is_inside_court(0.0, -10.01))


func test_team_a_is_on_positive_z() -> void:
	assert_eq(CourtSpec.half_of(3.0), CourtSpec.Half.A)
	assert_eq(CourtSpec.half_of(-3.0), CourtSpec.Half.B)
