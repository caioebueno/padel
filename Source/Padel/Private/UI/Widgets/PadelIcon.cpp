#include "UI/Widgets/PadelIcon.h"

#include "Rendering/DrawElements.h"

namespace
{
	using FIconPath = TArray<FVector2f>;

	// Points on a 32 x 32 grid.
	void IconArc(FIconPath& Path, float CX, float CY, float R, float FromDeg, float ToDeg, int32 Segments = 16)
	{
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(FMath::Lerp(FromDeg, ToDeg, static_cast<float>(Index) / Segments));
			Path.Add(FVector2f(CX + R * FMath::Cos(Angle), CY + R * FMath::Sin(Angle)));
		}
	}

	void IconPaths(EPadelIcon Icon, TArray<FIconPath>& Out)
	{
		switch (Icon)
		{
		case EPadelIcon::Padlock:
		{
			Out.Add({{8, 15}, {24, 15}, {24, 28}, {8, 28}, {8, 15}});
			FIconPath Shackle;
			Shackle.Add(FVector2f(11, 15));
			IconArc(Shackle, 16, 11, 5, 180, 360, 10);
			Shackle.Add(FVector2f(21, 15));
			Out.Add(Shackle);
			Out.Add({{16, 20}, {16, 23}});
			break;
		}
		case EPadelIcon::Trophy:
		{
			FIconPath Cup;
			Cup.Add(FVector2f(9, 5));
			IconArc(Cup, 16, 13, 7, 180, 0, 12);
			Cup.Add(FVector2f(23, 5));
			Cup.Add(FVector2f(9, 5));
			Out.Add(Cup);
			FIconPath LeftHandle;
			IconArc(LeftHandle, 9, 10, 4, 270, 90, 8);
			Out.Add(LeftHandle);
			FIconPath RightHandle;
			IconArc(RightHandle, 23, 10, 4, -90, 90, 8);
			Out.Add(RightHandle);
			Out.Add({{16, 20}, {16, 25}});
			Out.Add({{10, 27}, {22, 27}});
			break;
		}
		case EPadelIcon::LinkedFigures:
		{
			for (const float CX : {11.0f, 21.0f})
			{
				FIconPath Head;
				IconArc(Head, CX, 9, 3.5f, 0, 360, 16);
				Out.Add(Head);
				FIconPath Body;
				IconArc(Body, CX, 25, 6, 180, 360, 12);
				Out.Add(Body);
			}
			Out.Add({{14, 21}, {18, 21}});
			break;
		}
		case EPadelIcon::Court:
			Out.Add({{5, 4}, {27, 4}, {27, 28}, {5, 28}, {5, 4}});
			Out.Add({{3, 16}, {29, 16}});
			Out.Add({{5, 9}, {27, 9}});
			Out.Add({{5, 23}, {27, 23}});
			Out.Add({{16, 9}, {16, 23}});
			break;
		case EPadelIcon::Ball:
		{
			FIconPath Outline;
			IconArc(Outline, 16, 16, 11, 0, 360, 24);
			Out.Add(Outline);
			FIconPath SeamLeft;
			IconArc(SeamLeft, 5, 16, 8, -60, 60, 10);
			Out.Add(SeamLeft);
			FIconPath SeamRight;
			IconArc(SeamRight, 27, 16, 8, 120, 240, 10);
			Out.Add(SeamRight);
			break;
		}
		case EPadelIcon::Check:
			Out.Add({{7, 17}, {13, 23}, {25, 9}});
			break;
		case EPadelIcon::ChevronLeft:
			Out.Add({{20, 7}, {11, 16}, {20, 25}});
			break;
		case EPadelIcon::ChevronRight:
			Out.Add({{12, 7}, {21, 16}, {12, 25}});
			break;
		case EPadelIcon::ShapeCross:
			Out.Add({{9, 9}, {23, 23}});
			Out.Add({{23, 9}, {9, 23}});
			break;
		case EPadelIcon::ShapeCircle:
		{
			FIconPath Ring;
			IconArc(Ring, 16, 16, 8, 0, 360, 24);
			Out.Add(Ring);
			break;
		}
		case EPadelIcon::ShapeSquare:
			Out.Add({{9, 9}, {23, 9}, {23, 23}, {9, 23}, {9, 9}});
			break;
		case EPadelIcon::ShapeTriangle:
			Out.Add({{16, 8}, {24, 22}, {8, 22}, {16, 8}});
			break;
		case EPadelIcon::None:
			break;
		}
	}
}

int32 UPadelIcon::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	TArray<FIconPath> Paths;
	IconPaths(Icon, Paths);
	if (Paths.IsEmpty())
	{
		return Layer;
	}
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const float Scale = FMath::Min(Size.X, Size.Y) / 32.0f;
	const FVector2f Offset = (Size - FVector2f(32.0f * Scale, 32.0f * Scale)) * 0.5f;
	const FLinearColor Colour = Tint * InWidgetStyle.GetColorAndOpacityTint();
	for (FIconPath& Path : Paths)
	{
		for (FVector2f& Point : Path)
		{
			Point = Offset + Point * Scale;
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(), Path,
			ESlateDrawEffect::None, Colour, true, Stroke * Scale);
	}
	return Layer + 1;
}
