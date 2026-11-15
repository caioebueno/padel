#include "Data/PadelBallMachineFeedRow.h"

Padel::Shots::FBallMachineFeed FPadelBallMachineFeedRow::ToCore(const FName& RowName) const
{
	Padel::Shots::FBallMachineFeed F;
	F.Name = TCHAR_TO_UTF8(*RowName.ToString());
	F.Drill = TCHAR_TO_UTF8(*Drill);
	F.OriginX = OriginX;
	F.OriginY = OriginY;
	F.OriginZ = OriginZ;
	F.TargetX = TargetX;
	F.TargetY = TargetY;
	F.TargetJitterM = TargetJitterM;
	F.SpeedKmh = SpeedKmh;
	F.ApexM = ApexM;
	F.IntervalS = IntervalS;
	return F;
}
