// Shared helpers for the native tests.
#pragma once

#include "BallPhysicsParams.h"

namespace Padel::Test
{
	// Data/Physics.json, loaded once. Tests use the real tuning file, so a broken data
	// file fails CI.
	const FBallPhysicsParams& Physics();
}
