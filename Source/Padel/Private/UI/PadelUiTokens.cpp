#include "UI/PadelUiTokens.h"

#include "Dom/JsonObject.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"

namespace PadelUi
{
	namespace
	{
		using Padel::Ui::EUiFontFace;
		using Padel::Ui::FUiTheme;
		using Padel::Ui::FUiTypeStyle;

		struct FTokenState
		{
			FUiTheme Theme = FUiTheme::Defaults();
			bool bLoaded = false;
			FUiOptions Options;
			FSimpleMulticastDelegate OnChanged;
		};

		FTokenState& TokenState()
		{
			static FTokenState State;
			return State;
		}

		// The Barlow faces, built once from the .ttf files. Deliberately never freed:
		// FStandaloneCompositeFont is a GC object and must not be destroyed after the
		// UObject system shuts down at exit.
		struct FBarlowFaces
		{
			TArray<TSharedPtr<const FCompositeFont>> Faces;
			bool bAll = false;
		};

		FBarlowFaces& BarlowFaces()
		{
			static FBarlowFaces* Faces = nullptr;
			if (!Faces)
			{
				Faces = new FBarlowFaces();
				Faces->bAll = true;
				const FString Dir = FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt"), TEXT("Fonts"));
				for (int32 Index = 0; Index < static_cast<int32>(EUiFontFace::Count); ++Index)
				{
					const FString File = FPaths::Combine(Dir, UTF8_TO_TCHAR(Padel::Ui::FontFaceFile(static_cast<EUiFontFace>(Index))));
					if (!IFileManager::Get().FileExists(*File))
					{
						Faces->Faces.Add(nullptr);
						Faces->bAll = false;
						continue;
					}
					Faces->Faces.Add(MakeShared<FStandaloneCompositeFont>(NAME_None, File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
				}
				if (!Faces->bAll)
				{
					UE_LOG(LogPadel, Warning, TEXT("Barlow fonts missing from %s; menus use the engine font (spec 4.2)."), *Dir);
				}
			}
			return *Faces;
		}

		FSlateFontInfo MakeFont(const FUiTypeStyle& Style, float Px)
		{
			const TArray<TSharedPtr<const FCompositeFont>>& Faces = BarlowFaces().Faces;
			const int32 FaceIndex = static_cast<int32>(Style.Face);
			FSlateFontInfo Info;
			if (Faces.IsValidIndex(FaceIndex) && Faces[FaceIndex].IsValid())
			{
				Info = FSlateFontInfo(Faces[FaceIndex], PxToFontSize(Px));
			}
			else
			{
				const bool bBold = Style.Face != EUiFontFace::Medium;
				Info = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::RoundToInt(PxToFontSize(Px)));
			}
			// Tracking is in 1/1000 em (spec gives it in percent).
			Info.LetterSpacing = FMath::RoundToInt(Style.TrackingPercent * 10.0f);
			return Info;
		}

		void ReadNumbers(const TSharedPtr<FJsonObject>& Root, const TCHAR* Group, FUiTheme& Theme)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Root->TryGetObjectField(Group, Object))
			{
				return;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
			{
				double Value = 0.0;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Value)
					|| !Theme.SetNumber(TCHAR_TO_UTF8(Group), TCHAR_TO_UTF8(*Pair.Key), static_cast<float>(Value)))
				{
					UE_LOG(LogPadel, Warning, TEXT("UiTheme.json: unknown or invalid %s.%s"), Group, *Pair.Key);
				}
			}
		}
	}

	bool LoadThemeFile(const FString& Path, FUiTheme& OutTheme)
	{
		OutTheme = FUiTheme::Defaults();
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *Path)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* Colours = nullptr;
		if (Root->TryGetObjectField(TEXT("Colour"), Colours))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Colours)->Values)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				FString Hex;
				double Alpha = 1.0;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !(*Entry)->TryGetStringField(TEXT("Hex"), Hex))
				{
					UE_LOG(LogPadel, Warning, TEXT("UiTheme.json: colour %s needs Hex and Alpha"), *Pair.Key);
					continue;
				}
				(*Entry)->TryGetNumberField(TEXT("Alpha"), Alpha);
				if (!OutTheme.SetColour(TCHAR_TO_UTF8(*Pair.Key), TCHAR_TO_UTF8(*Hex), static_cast<float>(Alpha)))
				{
					UE_LOG(LogPadel, Warning, TEXT("UiTheme.json: unknown or invalid colour %s"), *Pair.Key);
				}
			}
		}

		const TSharedPtr<FJsonObject>* Types = nullptr;
		if (Root->TryGetObjectField(TEXT("Type"), Types))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Types)->Values)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				FString Face;
				FUiTypeStyle Style;
				double Number = 0.0;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !(*Entry)->TryGetStringField(TEXT("Face"), Face)
					|| !Padel::Ui::FontFaceFromKey(TCHAR_TO_UTF8(*Face), Style.Face))
				{
					UE_LOG(LogPadel, Warning, TEXT("UiTheme.json: type %s has no known Face"), *Pair.Key);
					continue;
				}
				if ((*Entry)->TryGetNumberField(TEXT("Size"), Number))
				{
					Style.Size = static_cast<float>(Number);
				}
				if ((*Entry)->TryGetNumberField(TEXT("LineHeight"), Number))
				{
					Style.LineHeight = static_cast<float>(Number);
				}
				if ((*Entry)->TryGetNumberField(TEXT("Tracking"), Number))
				{
					Style.TrackingPercent = static_cast<float>(Number);
				}
				(*Entry)->TryGetBoolField(TEXT("Upper"), Style.bUpper);
				(*Entry)->TryGetBoolField(TEXT("Scales"), Style.bScales);
				if (!OutTheme.SetType(TCHAR_TO_UTF8(*Pair.Key), Style))
				{
					UE_LOG(LogPadel, Warning, TEXT("UiTheme.json: unknown type token %s"), *Pair.Key);
				}
			}
		}

		const TSharedPtr<FJsonObject>* Scales = nullptr;
		if (Root->TryGetObjectField(TEXT("TextScale"), Scales))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Scales)->Values)
			{
				double Value = 0.0;
				if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Value)
					|| !OutTheme.SetTextScale(TCHAR_TO_UTF8(*Pair.Key), static_cast<float>(Value)))
				{
					UE_LOG(LogPadel, Warning, TEXT("UiTheme.json: unknown text scale %s"), *Pair.Key);
				}
			}
		}

		ReadNumbers(Root, TEXT("Layout"), OutTheme);
		ReadNumbers(Root, TEXT("Motion"), OutTheme);
		ReadNumbers(Root, TEXT("Input"), OutTheme);

		// Card stat groups: replace the attribute lists, keeping the spec's row order
		// (JSON objects don't keep theirs).
		const TSharedPtr<FJsonObject>* CardStats = nullptr;
		if (Root->TryGetObjectField(TEXT("PlayerCardStats"), CardStats))
		{
			for (auto& [Label, Attributes] : OutTheme.PlayerCardStats)
			{
				const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
				if (!(*CardStats)->TryGetArrayField(UTF8_TO_TCHAR(Label.c_str()), List))
				{
					continue;
				}
				Attributes.clear();
				for (const TSharedPtr<FJsonValue>& Value : *List)
				{
					FString Name;
					if (Value.IsValid() && Value->TryGetString(Name))
					{
						Attributes.push_back(TCHAR_TO_UTF8(*Name));
					}
				}
			}
		}
		return true;
	}

	const FUiTheme& Theme()
	{
		FTokenState& State = TokenState();
		if (!State.bLoaded)
		{
			State.bLoaded = true;
			const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("UiTheme.json"));
			if (!LoadThemeFile(Path, State.Theme))
			{
				UE_LOG(LogPadel, Warning, TEXT("Could not read %s; menus use the built-in tokens (spec 7.12)."), *Path);
			}
		}
		return State.Theme;
	}

	void ReloadTheme()
	{
		TokenState().bLoaded = false;
		Theme();
		TokenState().OnChanged.Broadcast();
	}

	FLinearColor Colour(EUiColour Token)
	{
		const Padel::Ui::FUiColour& C = Theme().Colour(Token);
		// Theme colours are sRGB hex values; Slate wants linear.
		FLinearColor Linear = FLinearColor::FromSRGBColor(FColor(
			static_cast<uint8>(FMath::RoundToInt(C.R * 255.0f)),
			static_cast<uint8>(FMath::RoundToInt(C.G * 255.0f)),
			static_cast<uint8>(FMath::RoundToInt(C.B * 255.0f))));
		Linear.A = C.A;
		return Linear;
	}

	FLinearColor Colour(EUiColour Token, float AlphaScale)
	{
		FLinearColor Linear = Colour(Token);
		Linear.A *= AlphaScale;
		return Linear;
	}

	FLinearColor TeamColour(int32 Team)
	{
		if (Team == 0)
		{
			return Colour(EUiColour::TeamLagoon);
		}
		if (Team == 1)
		{
			return Colour(EUiColour::TeamEmber);
		}
		return Colour(EUiColour::TextPrimary);
	}

	FSlateFontInfo Font(EUiType Token)
	{
		return MakeFont(Theme().Type(Token), Theme().FontSize(Token, Options().TextSize));
	}

	FSlateFontInfo FontAtPx(EUiType Token, float Px)
	{
		return MakeFont(Theme().Type(Token), Px);
	}

	bool IsUpper(EUiType Token)
	{
		return Theme().Type(Token).bUpper;
	}

	bool HasBarlow()
	{
		return BarlowFaces().bAll;
	}

	const FUiOptions& Options()
	{
		return TokenState().Options;
	}

	void SetOptions(const FUiOptions& NewOptions)
	{
		FTokenState& State = TokenState();
		State.Options = NewOptions;
		State.OnChanged.Broadcast();
	}

	FSimpleMulticastDelegate& OnOptionsChanged()
	{
		return TokenState().OnChanged;
	}
}
