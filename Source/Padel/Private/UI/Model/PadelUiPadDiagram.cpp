#include "UI/Model/PadelUiPadDiagram.h"

#include <cmath>

namespace Padel::Ui
{
	namespace
	{
		constexpr float PadPi = 3.14159265358979f;
		constexpr int PadCurveSegments = 12;
		constexpr int PadCircleSegments = 32;
		constexpr int PadCornerSegments = 4;

		void PadQuad(FDiagramLine& Line, FDiagramPoint P0, FDiagramPoint P1, FDiagramPoint P2)
		{
			for (int Step = 1; Step <= PadCurveSegments; ++Step)
			{
				const float T = static_cast<float>(Step) / PadCurveSegments;
				const float U = 1.0f - T;
				Line.push_back({U * U * P0.X + 2.0f * U * T * P1.X + T * T * P2.X, U * U * P0.Y + 2.0f * U * T * P1.Y + T * T * P2.Y});
			}
		}

		void PadCubic(FDiagramLine& Line, FDiagramPoint P1, FDiagramPoint P2, FDiagramPoint P3)
		{
			const FDiagramPoint P0 = Line.back();
			for (int Step = 1; Step <= PadCurveSegments; ++Step)
			{
				const float T = static_cast<float>(Step) / PadCurveSegments;
				const float U = 1.0f - T;
				const float A = U * U * U;
				const float B = 3.0f * U * U * T;
				const float C = 3.0f * U * T * T;
				const float D = T * T * T;
				Line.push_back({A * P0.X + B * P1.X + C * P2.X + D * P3.X, A * P0.Y + B * P1.Y + C * P2.Y + D * P3.Y});
			}
		}

		FDiagramLine PadCircle(float CX, float CY, float R)
		{
			FDiagramLine Line;
			for (int Step = 0; Step <= PadCircleSegments; ++Step)
			{
				const float Angle = 2.0f * PadPi * static_cast<float>(Step) / PadCircleSegments;
				Line.push_back({CX + R * std::cos(Angle), CY + R * std::sin(Angle)});
			}
			return Line;
		}

		FDiagramLine PadRoundedRect(float X, float Y, float W, float H, float R)
		{
			FDiagramLine Line;
			auto Corner = [&Line, R](float CX, float CY, float FromDeg)
			{
				for (int Step = 0; Step <= PadCornerSegments; ++Step)
				{
					const float Angle = (FromDeg + 90.0f * static_cast<float>(Step) / PadCornerSegments) * PadPi / 180.0f;
					Line.push_back({CX + R * std::cos(Angle), CY + R * std::sin(Angle)});
				}
			};
			Corner(X + W - R, Y + R, -90.0f);
			Corner(X + W - R, Y + H - R, 0.0f);
			Corner(X + R, Y + H - R, 90.0f);
			Corner(X + R, Y + R, 180.0f);
			Line.push_back(Line.front());
			return Line;
		}

		std::vector<FDiagramLine> BuildPadOutline()
		{
			std::vector<FDiagramLine> Lines;

			// The body (board path): a soft bar with two grips.
			FDiagramLine Body{{720, 380}};
			PadQuad(Body, {720, 380}, {960, 352}, {1200, 380});
			PadCubic(Body, {1290, 392}, {1330, 440}, {1350, 520});
			PadCubic(Body, {1375, 620}, {1390, 700}, {1330, 722});
			PadCubic(Body, {1280, 737}, {1240, 690}, {1200, 642});
			PadCubic(Body, {1150, 602}, {770, 602}, {720, 642});
			PadCubic(Body, {680, 690}, {640, 737}, {590, 722});
			PadCubic(Body, {530, 700}, {545, 620}, {570, 520});
			PadCubic(Body, {590, 440}, {630, 392}, {720, 380});
			Lines.push_back(Body);

			// Triggers, then shoulder buttons.
			Lines.push_back(PadRoundedRect(1120, 322, 90, 28, 10));
			Lines.push_back(PadRoundedRect(710, 322, 90, 28, 10));
			Lines.push_back(PadRoundedRect(690, 352, 130, 20, 8));
			Lines.push_back(PadRoundedRect(1100, 352, 130, 20, 8));

			// Sticks: a well and a cap each.
			Lines.push_back(PadCircle(790, 470, 44));
			Lines.push_back(PadCircle(790, 470, 28));
			Lines.push_back(PadCircle(1050, 560, 44));
			Lines.push_back(PadCircle(1050, 560, 28));

			// D-pad.
			Lines.push_back({{859, 520}, {881, 520}, {881, 549}, {910, 549}, {910, 571}, {881, 571}, {881, 600}, {859, 600}, {859, 571},
				{830, 571}, {830, 549}, {859, 549}, {859, 520}});

			// Face buttons.
			Lines.push_back(PadCircle(1130, 428, 20));
			Lines.push_back(PadCircle(1086, 470, 20));
			Lines.push_back(PadCircle(1174, 470, 20));
			Lines.push_back(PadCircle(1130, 512, 20));

			// The two small menu buttons.
			Lines.push_back(PadRoundedRect(985, 423, 30, 18, 9));
			Lines.push_back(PadRoundedRect(905, 423, 30, 18, 9));
			return Lines;
		}

		std::vector<FPadCallout> BuildPadCallouts()
		{
			// Board "Controls7": leaders leave the labels 16 px from their edge, run 64 px
			// level, then angle to the control.
			return {
				{"Gamepad_LeftShoulder", ECalloutSide::Left, 290, {{536, 290}, {600, 290}, {755, 362}}},
				{"Gamepad_Left2D", ECalloutSide::Left, 380, {{536, 380}, {600, 380}, {790, 470}}},
				{"Gamepad_DPad_Up", ECalloutSide::Left, 470, {{536, 470}, {600, 470}, {870, 528}}},
				{"Gamepad_DPad_Left", ECalloutSide::Left, 550, {{536, 550}, {600, 550}, {838, 560}}},
				{"Gamepad_DPad_Right", ECalloutSide::Left, 630, {{536, 630}, {600, 630}, {902, 560}}},
				{"Gamepad_DPad_Down", ECalloutSide::Left, 710, {{536, 710}, {600, 710}, {870, 592}}},
				{"Gamepad_RightTrigger", ECalloutSide::Right, 290, {{1384, 290}, {1320, 290}, {1165, 336}}},
				{"Gamepad_RightShoulder", ECalloutSide::Right, 370, {{1384, 370}, {1320, 370}, {1165, 362}}},
				{"Gamepad_FaceButton_Top", ECalloutSide::Right, 450, {{1384, 450}, {1320, 450}, {1130, 428}}},
				{"Gamepad_FaceButton_Right", ECalloutSide::Right, 530, {{1384, 530}, {1320, 530}, {1174, 470}}},
				{"Gamepad_FaceButton_Bottom", ECalloutSide::Right, 610, {{1384, 610}, {1320, 610}, {1130, 512}}},
				{"Gamepad_FaceButton_Left", ECalloutSide::Right, 690, {{1384, 690}, {1100, 690}, {1100, 470}, {1086, 470}}},
				{"Gamepad_Special_Right", ECalloutSide::Top, 258, {{1000, 286}, {1000, 432}}},
			};
		}
	}

	const FPadDiagramLayout& PadDiagramLayout()
	{
		static const FPadDiagramLayout Layout;
		return Layout;
	}

	const std::vector<FDiagramLine>& PadOutline()
	{
		static const std::vector<FDiagramLine> Lines = BuildPadOutline();
		return Lines;
	}

	const std::vector<FPadCallout>& PadCallouts()
	{
		static const std::vector<FPadCallout> Callouts = BuildPadCallouts();
		return Callouts;
	}

	const FPadCallout* FindPadCallout(const std::string& KeyName)
	{
		for (const FPadCallout& Callout : PadCallouts())
		{
			if (Callout.KeyName == KeyName)
			{
				return &Callout;
			}
		}
		return nullptr;
	}
}
