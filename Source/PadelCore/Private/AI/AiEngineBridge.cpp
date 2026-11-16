#include "AI/AiEngineBridge.h"

namespace Padel::AI::Bridge
{
EShotType ToEngine(EAiShot Shot)
{
	switch (Shot)
	{
	case EAiShot::Drive: return EShotType::Drive;
	case EAiShot::Volley: return EShotType::Volley;
	case EAiShot::Lob: return EShotType::Lob;
	case EAiShot::Bandeja: return EShotType::Bandeja;
	case EAiShot::Vibora: return EShotType::Vibora;
	case EAiShot::Smash: return EShotType::Smash;
	case EAiShot::Chiquita: return EShotType::Chiquita;
	case EAiShot::ServeFlat: return EShotType::ServeFlat;
	case EAiShot::ServeKick: return EShotType::ServeKick;
	}
	return EShotType::Drive;
}

EAiShot ToAi(EShotType Shot)
{
	switch (Shot)
	{
	case EShotType::Volley: return EAiShot::Volley;
	case EShotType::Lob: return EAiShot::Lob;
	case EShotType::Bandeja: return EAiShot::Bandeja;
	case EShotType::Vibora: return EAiShot::Vibora;
	case EShotType::Smash: return EAiShot::Smash;
	case EShotType::Chiquita: return EAiShot::Chiquita;
	case EShotType::ServeFlat: return EAiShot::ServeFlat;
	case EShotType::ServeKick: return EAiShot::ServeKick;
	default: return EAiShot::Drive;
	}
}

FAiPlayerProfile ProfileFromRatings(const Shots::FPlayerRatings& R)
{
	FAiPlayerProfile P;
	P.Speed = R.Speed;
	P.Acceleration = R.Acceleration;
	P.Reach = R.Reach;
	P.Jump = R.Jump;
	P.WallPlay = R.WallPlay;
	P.Anticipation = R.Anticipation;
	P.Positioning = R.Positioning;
	P.Teamwork = R.Teamwork;
	P.ReturnSkill = R.ReturnSkill;
	P.PreferredSide = R.PreferredSide == Shots::ECourtSide::Left ? ESide::Left : ESide::Right;
	P.Handedness = R.Handedness == Shots::EHandedness::Left ? EHand::Left : EHand::Right;
	return P;
}

Shots::FPlayerRatings WithOffset(const Shots::FPlayerRatings& Ratings, int Offset)
{
	Shots::FPlayerRatings Out = Ratings;
	for (int Index = 0; Index < static_cast<int>(Shots::ERating::Count); ++Index)
	{
		const Shots::ERating Rating = static_cast<Shots::ERating>(Index);
		const int Value = Out.Get(Rating) + Offset;
		Out.Set(Rating, Value < 1 ? 1 : (Value > 99 ? 99 : Value));
	}
	return Out;
}
}
