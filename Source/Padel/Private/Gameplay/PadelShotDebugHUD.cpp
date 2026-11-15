#include "Gameplay/PadelShotDebugHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Gameplay/PadelPracticeDirector.h"
#include "Shots/SwingPhase.h"

void APadelShotDebugHUD::DrawHUD()
{
	Super::DrawHUD();
#if !UE_BUILD_SHIPPING
	TActorIterator<APadelPracticeDirector> It(GetWorld());
	if (!It || !Canvas)
	{
		return;
	}
	const APadelPracticeDirector* Director = *It;
	const FPadelShotDebugInfo& Info = Director->GetDebugInfo();
	const APadelPlayerPawn* Pawn = Director->GetPlayerPawn();
	const Padel::Shots::FPlayerShotController* Player = Pawn ? Pawn->GetShotController() : nullptr;

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("PRACTICE  drill: %s%s   (D-pad left/right or 3/4: drill, up/1: pause, down/2: feed)"),
		*Info.Drill, Info.bMachinePaused ? TEXT(" [paused]") : TEXT("")));
	if (Player)
	{
		Lines.Add(FString::Printf(TEXT("swing: %s  charge %3.0f%%  ideal in %+.0f ms  stamina %3.0f%%  split-steps %d%s"),
			UTF8_TO_TCHAR(Padel::Shots::SwingPhaseName(Player->GetPhase())), Player->GetCharge() * 100.0,
			Player->HasIdealMoment() ? Player->TimeToIdealS() * 1000.0 : 0.0, Player->GetMotion().GetStamina() * 100.0,
			Info.SplitSteps, Player->IsAwaitingServe() ? TEXT("  SERVE: press A/J to toss") : TEXT("")));
	}
	if (Info.bHasShot)
	{
		const Padel::Shots::FShotIntent& S = Info.LastShot;
		Lines.Add(FString::Printf(TEXT("last: %s  %s (%+.0f ms)  %.0f km/h  charge %.0f%%  scatter %.2f m%s%s"),
			UTF8_TO_TCHAR(Padel::ShotTypeName(S.ShotType)), UTF8_TO_TCHAR(Padel::TimingGradeName(S.TimingGrade)),
			S.TimingOffsetS * 1000.0, S.SpeedKmh(), S.Charge * 100.0, S.ScatterRadiusM,
			S.bWallShot ? TEXT("  off the glass") : TEXT(""), S.bVolley ? TEXT("  volley") : TEXT("")));
		Lines.Add(FString::Printf(TEXT("target (m): aimed %.1f, %.1f  actual %.1f, %.1f  apex %.1f"),
			S.AimedTarget.X, S.AimedTarget.Y, S.Target.X, S.Target.Y, S.ApexHeightM));
	}
	if (Info.LastOutcome == Padel::Shots::ESwingOutcome::Whiff)
	{
		Lines.Add(TEXT("last swing: whiff (out of reach)"));
	}
	else if (Info.LastOutcome == Padel::Shots::ESwingOutcome::ShadowSwing)
	{
		Lines.Add(TEXT("last swing: no ball to hit"));
	}
	Lines.Add(FString::Printf(TEXT("hits %d  perfect %d"), Info.Hits, Info.Perfects));

	float Y = 40.0f;
	for (const FString& Line : Lines)
	{
		Canvas->SetDrawColor(FColor::White);
		Canvas->DrawText(GEngine->GetSmallFont(), Line, 40.0f, Y);
		Y += 18.0f;
	}
#endif
}
