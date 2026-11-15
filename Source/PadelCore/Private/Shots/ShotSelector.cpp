#include "Shots/ShotSelector.h"

namespace Padel::Shots
{
	// Named, not anonymous: Unreal unity builds merge .cpp files.
	namespace ShotSelectorDetail
	{
		FShotChoice Of(EShotType Type, double ExtraErrorMult = 1.0)
		{
			return FShotChoice{ Type, ExtraErrorMult };
		}
		FShotChoice Groundstroke(const FShotContext& C, const FShotLibrary& Library)
		{
			if (C.bModTouch && C.ContactPosition.Z <= Library.Get(EShotType::Chiquita).ContactHeightMaxM)
			{
				return Of(EShotType::Chiquita);
			}
			// GDD 4: the drive button before the bounce is a volley.
			return Of(C.bBallBounced ? EShotType::Drive : EShotType::Volley);
		}

		FShotChoice Overhead(const FShotContext& C, const FShotModel& Model)
		{
			if (C.bModAttack)
			{
				if (C.IsForehand())
				{
					return Of(EShotType::Vibora);
				}
				// GDD 4: RB + Y on the backhand is a backhand overhead, a less accurate bandeja.
				return Of(EShotType::Bandeja, Model.BackhandOverheadErrorMult);
			}
			if (C.Charge >= Model.SmashMinCharge)
			{
				if (C.bSet)
				{
					return Of(EShotType::Smash);
				}
				// GDD 4: a full charge without being set becomes a bandeja with extra error.
				return Of(EShotType::Bandeja, Model.UnsetFullChargeErrorMult);
			}
			return Of(EShotType::Bandeja);
		}
	}
	using namespace ShotSelectorDetail;

	FShotChoice FShotSelector::Select(const FShotContext& C, const FShotLibrary& Library)
	{
		if (C.bServe)
		{
			// GDD 4: RB turns the serve into a kick serve.
			return Of(C.bModAttack ? EShotType::ServeKick : EShotType::ServeFlat);
		}
		const double H = C.ContactPosition.Z;
		switch (C.Button)
		{
		case EShotButton::Lob:
			if (H > Library.Get(EShotType::Lob).ContactHeightMaxM)
			{
				return Of(EShotType::Bandeja);
			}
			return Of(EShotType::Lob);
		case EShotButton::Overhead:
			if (H < Library.Get(EShotType::Bandeja).ContactHeightMinM)
			{
				return Groundstroke(C, Library);
			}
			return Overhead(C, Library.Model);
		default:
			return Groundstroke(C, Library);
		}
	}
}
