// The Controls screen's pad diagram (docs/ui/MENU_SPEC.md 7.6, design board "Controls7"):
// a neutral line drawing of a generic two-stick pad, our own and no manufacturer's
// outline, and where each control's label sits with its leader line. All in the spec's
// 1920 x 1080 px. UPadelPadDiagram draws it; the Controls screen places the labels.
//
// Callouts are keyed by the pad key at that spot, not by the action, so a label follows
// its binding: the screen looks up which action is bound to each key.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <string>
#include <vector>

namespace Padel::Ui
{
	struct FDiagramPoint
	{
		float X = 0.0f;
		float Y = 0.0f;
	};

	using FDiagramLine = std::vector<FDiagramPoint>;

	enum class ECalloutSide
	{
		Left,  // label right-aligned, ending at LeftLabelRight
		Right, // label left-aligned, starting at RightLabelLeft
		Top,   // label centred on TopLabelX, TopLabelY
	};

	struct FPadCallout
	{
		// The Unreal pad key at this spot, e.g. "Gamepad_LeftShoulder".
		std::string KeyName;
		ECalloutSide Side = ECalloutSide::Left;
		// The label's vertical centre (Left and Right), in px.
		float LabelY = 0.0f;
		// From the label's side to the control; the last point gets the dot.
		FDiagramLine Leader;
	};

	struct FPadDiagramLayout
	{
		// Board "Controls7": labels end 16 px before their leader starts.
		float LeftLabelRight = 520.0f;
		float RightLabelLeft = 1400.0f;
		float TopLabelX = 1000.0f;
		float TopLabelY = 258.0f;
		float LabelGap = 16.0f;
		float FooterY = 800.0f;
		float DotRadius = 5.0f;
		float Stroke = 2.0f;
		// Spec 7.6: the diagram lives in C3-C10, y 232-760.
		float AreaTop = 232.0f;
		float AreaBottom = 760.0f;
	};

	const FPadDiagramLayout& PadDiagramLayout();

	// The pad's strokes: body outline, shoulders and triggers, sticks, D-pad, face
	// buttons and the two small menu buttons. Curves are already sampled into points.
	const std::vector<FDiagramLine>& PadOutline();

	// Every labelled control, left column top to bottom, then right, then Start on top.
	const std::vector<FPadCallout>& PadCallouts();
	// The callout at a key, or nullptr if the diagram doesn't show that key.
	const FPadCallout* FindPadCallout(const std::string& KeyName);
}
