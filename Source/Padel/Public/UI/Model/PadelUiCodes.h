// Labels for the engine's codes: point reasons, serve faults, shot types, timing grades.
//
// Part of the UI model: plain C++20 with no Unreal headers, so the native test suite
// (Tests/Native) builds and checks it on every PR without an engine install.
//
// Codes are the GDD names ("DOUBLE_BOUNCE", "PERFECT", "BANDEJA"), as PadelCore's
// PointReasonCode(), ServeFaultCode(), TimingGradeName() and ShotTypeName() return them
// (Agent 2). Matching ignores case. The Unreal layer turns an enum into its name once,
// at the event boundary, so the UI never depends on enum order.

#pragma once

#include <string>
#include <string_view>

namespace Padel::Ui
{
	inline constexpr int TeamA = 0;
	inline constexpr int TeamB = 1;

	// "Double bounce", "Glass on the fly"... Unknown codes are turned into sentence case.
	std::string ReasonLabel(std::string_view Reason);

	// True when the point was won by the winning team's own shot (a winner); every other
	// ending is an error by the team that lost the point (GDD 3.3).
	bool IsWinnerReason(std::string_view Reason);

	// "Fault" plus what went wrong, e.g. "Fault: mesh after bounce" (GDD 3.2).
	std::string ServeFaultLabel(std::string_view Fault);

	// "Perfect", "Good", "Early", "Late", "Miss" (GDD 5.1).
	std::string TimingLabel(std::string_view Grade);

	// "Drive", "Víbora", "Kick serve"... (GDD 4).
	std::string ShotLabel(std::string_view Shot);

	// Players 0 and 1 are Team A, 2 and 3 Team B. -1 when out of range.
	int TeamOfPlayer(int PlayerId);

	int OtherTeam(int Team);

	// "bandeja" or "Double bounce" -> "BANDEJA", "DOUBLE_BOUNCE": codes compare in this form.
	std::string NormaliseCode(std::string_view Code);

	// "SOME_CODE" -> "Some code".
	std::string SentenceCase(std::string_view Code);
}
