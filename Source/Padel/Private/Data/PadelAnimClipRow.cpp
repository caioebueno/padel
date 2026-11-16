#include "Data/PadelAnimClipRow.h"

bool FPadelAnimClipRow::ToCore(FName RowName, Padel::Animation::FAnimClip& Out, FString& OutError) const
{
	using Padel::Animation::FAnimClipLibrary;
	Out.Name = TCHAR_TO_UTF8(*RowName.ToString());
	if (!FAnimClipLibrary::ParseShot(TCHAR_TO_UTF8(*Shot), Out.Shot) || !FAnimClipLibrary::ParseSide(TCHAR_TO_UTF8(*Side), Out.Side)
		|| !FAnimClipLibrary::ParseVariant(TCHAR_TO_UTF8(*Variant), Out.Variant))
	{
		OutError = FString::Printf(TEXT("AnimClips.json %s: unknown Shot, Side or Variant (%s, %s, %s)"), *RowName.ToString(), *Shot, *Side, *Variant);
		return false;
	}
	Out.Asset = TCHAR_TO_UTF8(*Asset);
	Out.WindupS = WindupS;
	Out.ContactS = ContactS;
	Out.EndS = EndS;
	Out.Backswing = { BackswingRightM, BackswingForwardM, BackswingUpM };
	Out.Follow = { FollowRightM, FollowForwardM, FollowUpM };
	return true;
}
