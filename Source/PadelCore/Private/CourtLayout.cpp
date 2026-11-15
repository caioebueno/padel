#include "CourtLayout.h"

namespace Padel::Court
{
	namespace LayoutBuild
	{
		// Placeholder build sizes, not regulation measurements.
		constexpr double FrameSize = 0.06;
		constexpr double LineThickness = 0.002;
		constexpr int NetSegments = 20;
		constexpr double NetThickness = 0.04;
		constexpr double NetPostSize = 0.08;
		constexpr double TapeHeight = 0.05;

		void AddCollider(std::vector<FCourtBox>& Out, std::string Name, FVec3 Centre, FVec3 Size,
			ESurface Surface, EZone Zone, EColorRole Color)
		{
			FCourtBox Box;
			Box.Name = std::move(Name);
			Box.Centre = Centre;
			Box.Size = Size;
			Box.Surface = Surface;
			Box.Zone = Zone;
			Box.bCollides = true;
			Box.bVisible = Color != EColorRole::Hidden;
			Box.Color = Color;
			Out.push_back(std::move(Box));
		}

		void AddVisual(std::vector<FCourtBox>& Out, std::string Name, FVec3 Centre, FVec3 Size, EColorRole Color)
		{
			FCourtBox Box;
			Box.Name = std::move(Name);
			Box.Centre = Centre;
			Box.Size = Size;
			Box.bCollides = false;
			Box.bVisible = true;
			Box.Color = Color;
			Out.push_back(std::move(Box));
		}

		// A see-through wall panel: an invisible collider plus a visible top rail and end posts,
		// so cameras see through glass and mesh until real materials exist.
		void AddWallPanel(std::vector<FCourtBox>& Out, const std::string& Name, FVec3 Centre, FVec3 Size, ESurface Surface)
		{
			AddCollider(Out, Name, Centre, Size, Surface, EZone::Court, EColorRole::Hidden);

			const bool bAlongX = Size.X > Size.Y;
			const FVec3 Rail = bAlongX ? FVec3{ Size.X, FrameSize, FrameSize } : FVec3{ FrameSize, Size.Y, FrameSize };
			AddVisual(Out, Name + "_TopRail", { Centre.X, Centre.Y, Centre.Z + Size.Z / 2.0 - FrameSize / 2.0 }, Rail, EColorRole::Frame);

			const FVec3 Post{ FrameSize, FrameSize, Size.Z };
			const double DX = bAlongX ? Size.X / 2.0 - FrameSize / 2.0 : 0.0;
			const double DY = bAlongX ? 0.0 : Size.Y / 2.0 - FrameSize / 2.0;
			AddVisual(Out, Name + "_PostA", { Centre.X + DX, Centre.Y + DY, Centre.Z }, Post, EColorRole::Frame);
			AddVisual(Out, Name + "_PostB", { Centre.X - DX, Centre.Y - DY, Centre.Z }, Post, EColorRole::Frame);
		}

		const char* HalfLabel(double Half) { return Half > 0.0 ? "A" : "B"; }
		const char* SideLabel(double Side) { return Side > 0.0 ? "YPos" : "YNeg"; }

		std::vector<FCourtBox> Build()
		{
			std::vector<FCourtBox> Out;
			const double T = WallThickness;
			const double G = GroundThickness;
			const double Signs[] = { 1.0, -1.0 };

			// --- Floor ---
			AddCollider(Out, "Ground", { 0.0, 0.0, -G / 2.0 }, { Length, Width, G }, ESurface::Ground, EZone::Court, EColorRole::Turf);

			// Run-off: a frame of floor around the court so players and the ball can leave
			// through the exits (out-of-court play).
			const double OuterLength = Length + 2.0 * RunoffBack;
			for (const double S : Signs)
			{
				AddCollider(Out, std::string("Runoff_Side") + SideLabel(S),
					{ 0.0, S * (HalfWidth + RunoffSide / 2.0), -G / 2.0 }, { OuterLength, RunoffSide, G },
					ESurface::Ground, EZone::Runoff, EColorRole::Runoff);
				AddCollider(Out, std::string("Runoff_Back") + HalfLabel(S),
					{ S * (HalfLength + RunoffBack / 2.0), 0.0, -G / 2.0 }, { RunoffBack, Width, G },
					ESurface::Ground, EZone::Runoff, EColorRole::Runoff);
			}

			// --- Lines (visual only) ---
			for (const double H : Signs)
			{
				AddVisual(Out, std::string("ServiceLine") + HalfLabel(H),
					{ H * ServiceLineX, 0.0, LineThickness / 2.0 }, { LineWidth, Width, LineThickness }, EColorRole::Line);
				const double CentreLength = ServiceLineX + CentreLineExtension;
				AddVisual(Out, std::string("CentreLine") + HalfLabel(H),
					{ H * CentreLength / 2.0, 0.0, LineThickness / 2.0 }, { CentreLength, LineWidth, LineThickness }, EColorRole::Line);
			}

			// --- Walls ---
			for (const double H : Signs)
			{
				const std::string HL = HalfLabel(H);
				const double BackX = H * (HalfLength + T / 2.0);
				// The back wall covers the corners too, so there is no gap where it meets the sides.
				const double BackWidth = Width + 2.0 * T;
				AddWallPanel(Out, "BackGlass" + HL, { BackX, 0.0, BackGlassHeight / 2.0 }, { T, BackWidth, BackGlassHeight }, ESurface::Glass);
				const double FenceHeight = FenceTotalHeight - BackGlassHeight;
				AddWallPanel(Out, "BackFence" + HL, { BackX, 0.0, BackGlassHeight + FenceHeight / 2.0 }, { T, BackWidth, FenceHeight }, ESurface::Mesh);

				for (const double S : Signs)
				{
					const std::string Label = HL + "_" + SideLabel(S);
					const double Y = S * (HalfWidth + T / 2.0);
					auto SidePanel = [&](const char* Name, double FromX, double ToX, double FromZ, double ToZ, ESurface Surface)
					{
						const double PanelLength = ToX - FromX;
						const double PanelHeight = ToZ - FromZ;
						AddWallPanel(Out, std::string(Name) + Label,
							{ H * (FromX + PanelLength / 2.0), Y, FromZ + PanelHeight / 2.0 },
							{ PanelLength, T, PanelHeight }, Surface);
					};
					// First 2 m from the back wall: glass to 3 m, mesh to 4 m.
					SidePanel("SideGlassFull", SideGlassStepStartX, HalfLength, 0.0, SideGlassFullHeight, ESurface::Glass);
					SidePanel("SideFenceAboveFull", SideGlassStepStartX, HalfLength, SideGlassFullHeight, FenceTotalHeight, ESurface::Mesh);
					// The step: glass to 2 m, mesh to 3 m.
					SidePanel("SideGlassStep", SideGlassEndX, SideGlassStepStartX, 0.0, SideGlassStepHeight, ESurface::Glass);
					SidePanel("SideFenceAboveStep", SideGlassEndX, SideGlassStepStartX, SideGlassStepHeight, SideMeshHeight, ESurface::Mesh);
					// The exit (ExitNearX .. ExitFarX) stays open to full height. Mesh from there to the net.
					SidePanel("SideFence", 0.0, ExitNearX, 0.0, SideMeshHeight, ESurface::Mesh);
				}
			}

			// --- Net: collision segments whose tops follow the sagging tape ---
			const double SegmentWidth = Width / NetSegments;
			for (int Index = 0; Index < NetSegments; ++Index)
			{
				const double CentreY = -HalfWidth + SegmentWidth * (Index + 0.5);
				const double Top = NetHeightAt(CentreY);
				const std::string Suffix = (Index < 10 ? "0" : "") + std::to_string(Index);
				AddCollider(Out, "NetSegment" + Suffix, { 0.0, CentreY, Top / 2.0 }, { NetThickness, SegmentWidth, Top },
					ESurface::Net, EZone::Court, EColorRole::Net);
				AddVisual(Out, "NetTape" + Suffix, { 0.0, CentreY, Top - TapeHeight / 2.0 },
					{ NetThickness + 0.004, SegmentWidth, TapeHeight }, EColorRole::Line);
			}
			for (const double S : Signs)
			{
				// The outer face of each post lines up with the side limit of the court.
				AddCollider(Out, std::string("NetPost") + SideLabel(S),
					{ 0.0, S * (HalfWidth - NetPostSize / 2.0), NetPostHeight / 2.0 }, { NetPostSize, NetPostSize, NetPostHeight },
					ESurface::Net, EZone::Court, EColorRole::Frame);
			}

			// --- Ceiling: invisible, only so traces and debug tools can find it ---
			AddCollider(Out, "Ceiling", { 0.0, 0.0, CeilingHeight + G / 2.0 }, { OuterLength, Width + 2.0 * RunoffSide, G },
				ESurface::Ceiling, EZone::Ceiling, EColorRole::Hidden);

			return Out;
		}

		// Slab test on one axis. Narrows [TMin, TMax]; returns false when the ray misses.
		bool ClipAxis(double Start, double Delta, double BoxMin, double BoxMax, double& TMin, double& TMax)
		{
			if (Delta == 0.0)
			{
				return Start >= BoxMin && Start <= BoxMax;
			}
			double T0 = (BoxMin - Start) / Delta;
			double T1 = (BoxMax - Start) / Delta;
			if (T0 > T1)
			{
				const double Temp = T0;
				T0 = T1;
				T1 = Temp;
			}
			TMin = T0 > TMin ? T0 : TMin;
			TMax = T1 < TMax ? T1 : TMax;
			return TMin <= TMax;
		}
	}

	const std::vector<FCourtBox>& BuildCourtLayout()
	{
		static const std::vector<FCourtBox> Layout = LayoutBuild::Build();
		return Layout;
	}

	std::optional<FRayHit> RaycastLayout(const std::vector<FCourtBox>& Layout, const FVec3& Start, const FVec3& End)
	{
		const FVec3 Delta{ End.X - Start.X, End.Y - Start.Y, End.Z - Start.Z };
		std::optional<FRayHit> Best;
		for (const FCourtBox& Box : Layout)
		{
			if (!Box.bCollides)
			{
				continue;
			}
			double TMin = 0.0;
			double TMax = 1.0;
			const bool bHit =
				LayoutBuild::ClipAxis(Start.X, Delta.X, Box.Centre.X - Box.Size.X / 2.0, Box.Centre.X + Box.Size.X / 2.0, TMin, TMax) &&
				LayoutBuild::ClipAxis(Start.Y, Delta.Y, Box.Centre.Y - Box.Size.Y / 2.0, Box.Centre.Y + Box.Size.Y / 2.0, TMin, TMax) &&
				LayoutBuild::ClipAxis(Start.Z, Delta.Z, Box.Centre.Z - Box.Size.Z / 2.0, Box.Centre.Z + Box.Size.Z / 2.0, TMin, TMax);
			// TMin == 0 means the segment starts inside the box: not an entry hit.
			if (!bHit || TMin <= 0.0)
			{
				continue;
			}
			if (!Best || TMin < Best->Time)
			{
				FRayHit Hit;
				Hit.Box = &Box;
				Hit.Surface = Box.Surface;
				Hit.Time = TMin;
				Hit.Point = { Start.X + Delta.X * TMin, Start.Y + Delta.Y * TMin, Start.Z + Delta.Z * TMin };
				Best = Hit;
			}
		}
		return Best;
	}
}
