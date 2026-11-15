extends GutTest
## QA contract checks between the GDD and the code (GDD 3.3, 3.4, 12 item 2).
##
## The reason codes and event names are read from docs/GDD.md itself, so a GDD
## change and the code can't drift apart silently. Until the engine pieces
## exist these tests report as pending; once scripts/engine/types.gd or the
## MatchEvents autoload lands, they become hard checks.

const GDD_PATH: String = "res://docs/GDD.md"
const TYPES_PATH: String = "res://scripts/engine/types.gd"
const MATCH_EVENTS_SETTING: String = "autoload/MatchEvents"
const TESTS_DIR: String = "res://tests"
const THIS_FILE: String = "res://tests/unit/test_gdd_contracts.gd"
## Rules from GDD 3.3 "Special cases" that also need their own reason code.
const SPECIAL_CASE_CODES: Array[String] = ["OUT_OF_COURT_WINNER"]


func _gdd_text() -> String:
	return FileAccess.get_file_as_string(GDD_PATH)


## The backticked codes in the first column of the GDD 3.3 "loses the point" table.
func _gdd_reason_codes() -> Array[String]:
	var text: String = _gdd_text()
	var start: int = text.find("### 3.3")
	var end: int = text.find("### 3.4")
	var section: String = text.substr(start, end - start)
	var codes: Array[String] = []
	var regex: RegEx = RegEx.create_from_string("(?m)^\\| `([A-Z_]+)` \\|")
	for m: RegExMatch in regex.search_all(section):
		codes.append(m.get_string(1))
	for code: String in SPECIAL_CASE_CODES:
		if section.contains("`%s`" % code):
			codes.append(code)
	return codes


## name -> argument count, parsed from the GDD 3.4 event list.
func _gdd_events() -> Dictionary[String, int]:
	var text: String = _gdd_text()
	var start: int = text.find("### 3.4")
	var end: int = text.find("\n## 4.")
	var section: String = text.substr(start, end - start)
	var events: Dictionary[String, int] = {}
	var regex: RegEx = RegEx.create_from_string("`([a-z_]+)\\(([^)]*)\\)`")
	for m: RegExMatch in regex.search_all(section):
		var args: String = m.get_string(2).strip_edges()
		events[m.get_string(1)] = 0 if args.is_empty() else args.split(",").size()
	return events


func _all_enum_keys(script: Script) -> Array[String]:
	var keys: Array[String] = []
	var constants: Dictionary = script.get_script_constant_map()
	for value: Variant in constants.values():
		if value is Dictionary:
			for key: Variant in (value as Dictionary).keys():
				keys.append(str(key))
	return keys


func _gd_files_under(dir_path: String) -> Array[String]:
	var files: Array[String] = []
	var dir: DirAccess = DirAccess.open(dir_path)
	if dir == null:
		return files
	for sub: String in dir.get_directories():
		if sub.begins_with("."):
			continue
		files.append_array(_gd_files_under(dir_path.path_join(sub)))
	for file: String in dir.get_files():
		if file.ends_with(".gd"):
			files.append(dir_path.path_join(file))
	return files


# --- The GDD parses (guards the checks below against a silent empty list) ---

func test_gdd_lists_the_reason_codes() -> void:
	var codes: Array[String] = _gdd_reason_codes()
	assert_gte(codes.size(), 13, "GDD 3.3 table parsed to %s" % [codes])
	assert_has(codes, "DOUBLE_BOUNCE")
	assert_has(codes, "VOLLEYED_SERVE")
	assert_has(codes, "OUT_OF_COURT_WINNER")


func test_gdd_lists_the_referee_events() -> void:
	var events: Dictionary[String, int] = _gdd_events()
	assert_eq(events.size(), 13, "GDD 3.4 parsed to %s" % [events])
	assert_eq(events.get("point_ended", -1), 3)
	assert_eq(events.get("serve_let", -1), 0)


# --- Contracts with the engine (pending until the engine lands) ---

func test_every_reason_code_is_in_types_enum() -> void:
	if not ResourceLoader.exists(TYPES_PATH):
		pending("scripts/engine/types.gd not added yet (Agent 2)")
		return
	var keys: Array[String] = _all_enum_keys(load(TYPES_PATH) as Script)
	for code: String in _gdd_reason_codes():
		assert_has(keys, code, "GDD 3.3 reason code %s missing from types.gd" % code)


func test_every_reason_code_has_a_test() -> void:
	# GDD 12 item 2: one or more tests for every row in section 3.
	if not ResourceLoader.exists(TYPES_PATH):
		pending("no reason codes in the engine yet (Agent 2)")
		return
	var corpus: String = ""
	for path: String in _gd_files_under(TESTS_DIR):
		if path != THIS_FILE:
			corpus += FileAccess.get_file_as_string(path)
	for code: String in _gdd_reason_codes():
		assert_true(corpus.contains(code), "no test mentions reason code %s" % code)


func test_match_events_has_every_gdd_signal() -> void:
	if not ProjectSettings.has_setting(MATCH_EVENTS_SETTING):
		pending("MatchEvents autoload not registered yet (Agent 2)")
		return
	var path: String = str(ProjectSettings.get_setting(MATCH_EVENTS_SETTING)).trim_prefix("*")
	var script: Script = load(path) as Script
	assert_not_null(script, "MatchEvents autoload must be a script")
	if script == null:
		return
	var declared: Dictionary[String, int] = {}
	for sig: Dictionary in script.get_script_signal_list():
		declared[str(sig["name"])] = (sig["args"] as Array).size()
	var events: Dictionary[String, int] = _gdd_events()
	for event_name: String in events:
		assert_true(declared.has(event_name), "MatchEvents lacks signal %s" % event_name)
		if declared.has(event_name):
			assert_eq(declared[event_name], events[event_name],
				"%s argument count differs from GDD 3.4" % event_name)
