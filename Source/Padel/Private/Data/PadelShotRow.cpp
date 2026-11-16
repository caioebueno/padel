#include "Data/PadelShotRow.h"

Padel::Shots::FShotParams FPadelShotRow::ToCore(Padel::EShotType Type) const
{
	using namespace Padel::Shots;
	FShotParams P;
	P.Type = Type;
	P.SpeedMinKmh = SpeedMinKmh;
	P.SpeedMaxKmh = SpeedMaxKmh;
	P.BaseSpinTop = BaseSpinTop;
	P.BaseSpinSide = BaseSpinSide;
	P.ApexMinM = ApexMinM;
	P.ApexMaxM = ApexMaxM;
	P.BaseErrorM = BaseErrorM;
	P.PerfectWindowMs = PerfectWindowMs;
	P.GoodWindowMs = GoodWindowMs;
	P.EarlyLateWindowMs = EarlyLateWindowMs;
	P.ForehandRating = RatingFromName(TCHAR_TO_UTF8(*ForehandRating.ToString()));
	P.BackhandRating = RatingFromName(TCHAR_TO_UTF8(*BackhandRating.ToString()));
	P.ContactHeightMinM = ContactHeightMinM;
	P.ContactHeightMaxM = ContactHeightMaxM;
	P.PreferredContactHeightM = PreferredContactHeightM;
	P.ContactHeightPerBodyHeight = ContactHeightPerBodyHeight;
	P.LowContactHeightM = LowContactHeightM;
	P.LowContactNetClearanceM = LowContactNetClearanceM;
	P.IdealContactSideM = IdealContactSideM;
	P.IdealContactForwardM = IdealContactForwardM;
	P.IdealContactSideBackM = IdealContactSideBackM;
	P.IdealContactForwardBackM = IdealContactForwardBackM;
	P.DepthShortM = DepthShortM;
	P.DepthDeepM = DepthDeepM;
	P.DepthNeutralM = DepthNeutralM;
	P.LateralMaxM = LateralMaxM;
	P.ForwardS = ForwardS;
	P.FollowThroughS = FollowThroughS;
	P.RecoveryS = RecoveryS;
	P.SwingMoveMult = SwingMoveMult;
	return P;
}
