#include "UI/PadelUiPaint.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace PadelUiPaint
{
	namespace
	{
		constexpr int32 SoftSize = 64;
		constexpr int32 VignetteSize = 128;
		constexpr int32 FadeHeight = 64;

		// White texels with the given alpha; the brush tint supplies the colour.
		UTexture2D* MakeAlphaTexture(const TCHAR* Name, int32 Width, int32 Height, TFunctionRef<float(int32, int32)> Alpha)
		{
			UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, FName(Name));
			if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
			{
				return nullptr;
			}
			Texture->SRGB = false;
			Texture->Filter = TF_Bilinear;
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
			FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
			uint8* Texels = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					uint8* Texel = Texels + (Y * Width + X) * 4;
					Texel[0] = 255;
					Texel[1] = 255;
					Texel[2] = 255;
					Texel[3] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Alpha(X, Y) * 255.0f), 0, 255));
				}
			}
			Mip.BulkData.Unlock();
			Texture->UpdateResource();
			// Shared by every menu widget for the whole session.
			Texture->AddToRoot();
			return Texture;
		}

		float SmoothEdge(float T)
		{
			const float C = FMath::Clamp(T, 0.0f, 1.0f);
			return C * C * (3.0f - 2.0f * C);
		}

		UTexture2D* SoftTexture()
		{
			static TWeakObjectPtr<UTexture2D> Cached;
			if (!Cached.IsValid())
			{
				// A soft halo outside a rounded box and clear inside it, like a CSS
				// box-shadow, which never paints under the element (so translucent glass
				// doesn't darken). Drawn as a 9-slice box whose margins are the falloff, the
				// element's edge sits halfway into each margin.
				Cached = MakeAlphaTexture(TEXT("PadelUi_SoftBox"), SoftSize, SoftSize, [](int32 X, int32 Y)
				{
					const float Half = SoftSize * 0.5f;
					const float EdgeX = FMath::Min(X + 0.5f, SoftSize - X - 0.5f) / Half;
					const float EdgeY = FMath::Min(Y + 0.5f, SoftSize - Y - 0.5f) / Half;
					// Distance in from the outer bound, rounding the corners.
					float Inward = FMath::Min(EdgeX, EdgeY);
					if (EdgeX < 1.0f && EdgeY < 1.0f)
					{
						Inward = 1.0f - FMath::Sqrt(FMath::Square(1.0f - EdgeX) + FMath::Square(1.0f - EdgeY));
					}
					return Inward >= 0.5f ? 0.0f : SmoothEdge(Inward / 0.5f);
				});
			}
			return Cached.Get();
		}

		UTexture2D* VignetteTexture()
		{
			static TWeakObjectPtr<UTexture2D> Cached;
			if (!Cached.IsValid())
			{
				// Spec board: radial-gradient(ellipse 70% 70%, clear -> 0.6): alpha grows with
				// the distance from the centre, measured in 70% of the screen's size.
				Cached = MakeAlphaTexture(TEXT("PadelUi_Vignette"), VignetteSize, VignetteSize, [](int32 X, int32 Y)
				{
					const float U = (X + 0.5f) / VignetteSize - 0.5f;
					const float V = (Y + 0.5f) / VignetteSize - 0.5f;
					return FMath::Min(1.0f, FMath::Sqrt(U * U + V * V) / 0.7f);
				});
			}
			return Cached.Get();
		}

		UTexture2D* FadeTexture()
		{
			static TWeakObjectPtr<UTexture2D> Cached;
			if (!Cached.IsValid())
			{
				Cached = MakeAlphaTexture(TEXT("PadelUi_FadeUp"), 1, FadeHeight, [](int32, int32 Y)
				{
					return (Y + 0.5f) / FadeHeight;
				});
			}
			return Cached.Get();
		}

		FSlateBrush TextureBrush(UTexture2D* Texture, const FLinearColor& Tint)
		{
			FSlateBrush Brush;
			if (Texture)
			{
				Brush.SetResourceObject(Texture);
				Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Brush.DrawAs = ESlateBrushDrawType::Image;
			}
			else
			{
				// No texture (a headless run without a renderer): a flat tint still reads.
				Brush.DrawAs = ESlateBrushDrawType::Image;
			}
			Brush.TintColor = FSlateColor(Tint);
			return Brush;
		}
	}

	FSlateBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
	}

	FSlateBrush Solid(const FLinearColor& Fill)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Fill);
		return Brush;
	}

	FSlateBrush SoftBox(const FLinearColor& Tint, float BlurPx)
	{
		FSlateBrush Brush = TextureBrush(SoftTexture(), Tint);
		// 9-slice with half the texture in each margin: BlurPx of falloff on each side.
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(0.5f);
		Brush.ImageSize = FVector2D(BlurPx * 2.0f, BlurPx * 2.0f);
		return Brush;
	}

	FSlateBrush Vignette(const FLinearColor& Tint)
	{
		return TextureBrush(VignetteTexture(), Tint);
	}

	FSlateBrush FadeUp(const FLinearColor& Tint)
	{
		return TextureBrush(FadeTexture(), Tint);
	}
}
