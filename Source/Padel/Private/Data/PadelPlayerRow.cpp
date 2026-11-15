#include "Data/PadelPlayerRow.h"

Padel::Shots::FPlayerRatings FPadelPlayerRow::ToCore() const
{
	using namespace Padel::Shots;
	FPlayerRatings P;
	P.Handedness = Handedness == FName(TEXT("Left")) ? EHandedness::Left : EHandedness::Right;
	P.PreferredSide = PreferredSide == FName(TEXT("Left")) ? ECourtSide::Left : ECourtSide::Right;
	P.HeightM = HeightM;
	P.Set(ERating::Drive, Drive);
	P.Set(ERating::Backhand, Backhand);
	P.Set(ERating::Volley, Volley);
	P.Set(ERating::Lob, Lob);
	P.Set(ERating::Overhead, Overhead);
	P.Set(ERating::Bandeja, Bandeja);
	P.Set(ERating::Vibora, Vibora);
	P.Set(ERating::WallPlay, WallPlay);
	P.Set(ERating::Touch, Touch);
	P.Set(ERating::Serve, Serve);
	P.Set(ERating::ReturnSkill, ReturnSkill);
	P.Set(ERating::Speed, Speed);
	P.Set(ERating::Acceleration, Acceleration);
	P.Set(ERating::Agility, Agility);
	P.Set(ERating::Power, Power);
	P.Set(ERating::Reach, Reach);
	P.Set(ERating::Jump, Jump);
	P.Set(ERating::Stamina, Stamina);
	P.Set(ERating::Anticipation, Anticipation);
	P.Set(ERating::Positioning, Positioning);
	P.Set(ERating::Composure, Composure);
	P.Set(ERating::Consistency, Consistency);
	P.Set(ERating::Teamwork, Teamwork);
	return P;
}
