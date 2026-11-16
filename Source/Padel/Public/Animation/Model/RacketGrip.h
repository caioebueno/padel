// Where the racket sits in the player's hand (docs/decisions/0008-racket.md).
//
// Plain C++20 with no Unreal headers, so the native tests check it on every PR.
// UPadelPlayerAnimComponent feeds it the hand's bones in the skeleton's reference pose
// and attaches SM_Racket to the hand bone with the result.
//
// The grip is worked out from where the bones are, not from how each bone's axes point,
// so the same numbers fit the placeholder skeleton and every MetaHuman body (their bone
// axes differ, their joint positions don't). In the reference pose (a T- or A-pose,
// facing +Y) the palm faces down and the thumb points forward, so:
//
//   F  along the hand, wrist to knuckles (or forearm to wrist when the skeleton has no
//      finger bones)
//   N  out of the palm: down, made square to F
//   T  across the palm towards the thumb: square to both, on the forward side
//
// The handle lies across the palm, in front of it by GripOutM. A continental grip (how
// most padel players hold the racket) tilts the head from straight across the palm
// towards the fingers by HeadTiltDeg and turns the face from the palm's plane by
// FaceRollDeg.

#pragma once

#include "VecMath.h"

#include <string>

namespace Padel::Json
{
	struct FValue;
}

namespace Padel::Animation
{
	// Loaded from Data/Racket.json ("Racket"). Defaults match that file.
	struct FRacketSettings
	{
		// Asset path of the racket mesh (not tuning; the pipeline makes it).
		std::string Mesh = "/Game/Padel/Equipment/SM_Racket.SM_Racket";
	// The middle of the hitting face up the handle from the grip centre (the mesh's origin):
	// where the placeholder swing arc puts the face before there is a skeletal body.
	double FaceCentreM = 0.27;
		// Bones the hand's frame is read from. The racket rides Animation.json's RacketBone.
		std::string ForearmBone = "lowerarm_r";
		std::string KnuckleBone = "middle_01_r";
		// Palm centre: this share of the way from the wrist to the knuckles, or (no finger
		// bones) PalmLengthM past the wrist along the forearm.
		double PalmShare = 0.55;
		double PalmLengthM = 0.05;
		// Handle axis in front of the palm (skin plus half the handle), and the grip centre
		// (the racket's origin) along the handle towards the head from the palm centre:
		// the hand holds the lower half of the grip, the butt just below the little finger.
		double GripOutM = 0.03;
		double GripAlongM = 0.03;
		double HeadTiltDeg = 35.0;
		double FaceRollDeg = 40.0;
	};

	// Reads a "Racket" object like Data/Racket.json's. Missing keys keep their defaults.
	// Returns false (and leaves Out as it was) when Root has no "Racket" object.
	bool ReadRacketSettings(const Json::FValue& Root, FRacketSettings& Out);

	// Parses the text of Data/Racket.json. On failure Out keeps its values and Error says why.
	bool ParseRacketSettings(const std::string& Text, FRacketSettings& Out, std::string& Error);

	// Joint positions in the skeleton's reference pose, in its component space, metres.
	struct FRacketGripInput
	{
		Court::FVec3 Forearm;
		Court::FVec3 Hand;
		Court::FVec3 Knuckle;
		bool bHasKnuckle = false;
		// The component's up and the way the character faces (Unreal: +Z and +Y).
		Court::FVec3 Up{ 0.0, 0.0, 1.0 };
		Court::FVec3 Forward{ 0.0, 1.0, 0.0 };
	};

	// The racket's frame in the same space: its origin (the grip centre) and unit axes.
	// AxisZ runs up the handle to the head, AxisY looks out of one face, AxisX = Y x Z.
	struct FRacketGripFrame
	{
		Court::FVec3 Origin;
		Court::FVec3 AxisX{ 1.0, 0.0, 0.0 };
		Court::FVec3 AxisY{ 0.0, 1.0, 0.0 };
		Court::FVec3 AxisZ{ 0.0, 0.0, 1.0 };
		Court::FVec3 PalmCentre;
		Court::FVec3 PalmNormal;
		// False when the bones are degenerate (forearm and hand in one place).
		bool bValid = false;
	};

	FRacketGripFrame SolveRacketGrip(const FRacketGripInput& In, const FRacketSettings& Settings);
}
