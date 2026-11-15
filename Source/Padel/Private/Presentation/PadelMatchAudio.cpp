#include "Presentation/PadelMatchAudio.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UPadelMatchAudio::Play(EPadelMatchSound Sound, const FVector& WorldLocation, bool bAtLocation) const
{
	const TObjectPtr<USoundBase>* Found = Sounds.Find(Sound);
	if (!Found || !*Found)
	{
		return;
	}
	UObject* Context = GetOuter();
	if (!Context || !Context->GetWorld())
	{
		return;
	}
	if (bAtLocation)
	{
		UGameplayStatics::PlaySoundAtLocation(Context, *Found, WorldLocation);
	}
	else
	{
		UGameplayStatics::PlaySound2D(Context, *Found);
	}
}

EPadelMatchSound UPadelMatchAudio::SoundForSurface(const FString& Surface)
{
	if (Surface.Equals(TEXT("glass"), ESearchCase::IgnoreCase))
	{
		return EPadelMatchSound::BounceGlass;
	}
	if (Surface.Equals(TEXT("mesh"), ESearchCase::IgnoreCase))
	{
		return EPadelMatchSound::BounceMesh;
	}
	if (Surface.Equals(TEXT("net"), ESearchCase::IgnoreCase))
	{
		return EPadelMatchSound::BounceNet;
	}
	return EPadelMatchSound::BounceGround;
}
