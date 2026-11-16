#include "UI/Widgets/PadelToast.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::EUiColour;
using Padel::Ui::EUiType;

namespace
{
	constexpr float ToastWidth = 480.0f;
	constexpr float ToastPaddingX = 24.0f;
	constexpr float ToastPaddingY = 12.0f;
}

void UPadelToast::BuildTree()
{
	MessageText = MakeTokenText(Message, EUiType::BodySmall, EUiColour::TextPrimary);
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetClipping(EWidgetClipping::ClipToBounds);
	UOverlay* Glass = MakeGlassPanel(MessageText, FMargin(ToastPaddingX, ToastPaddingY), PadelUi::Theme().Layout.RadiusControl);
	WidgetTree->RootWidget = MakeSized(Glass, ToastWidth, 0.0f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UPadelToast::Show(const FText& InMessage)
{
	Message = InMessage;
	if (MessageText)
	{
		MessageText->SetText(Message);
	}
	const Padel::Ui::FUiMotion& Motion = PadelUi::Theme().Motion;
	Remaining = Motion.ToastSeconds + Motion.ToastFade;
	SetRenderOpacity(1.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPadelToast::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Remaining <= 0.0f)
	{
		return;
	}
	Remaining -= InDeltaTime;
	const float FadeTime = PadelUi::Theme().Motion.ToastFade;
	SetRenderOpacity(FadeTime > 0.0f ? FMath::Clamp(Remaining / FadeTime, 0.0f, 1.0f) : 1.0f);
	if (Remaining <= 0.0f)
	{
		Remaining = 0.0f;
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
