#include "Shots/PlayerRatings.h"

namespace Padel::Shots
{
	// Named, not anonymous: Unreal unity builds merge .cpp files.
	namespace PlayerRatingsDetail
	{
		constexpr const char* Names[] = {
			"Drive", "Backhand", "Volley", "Lob", "Overhead", "Bandeja", "Vibora", "WallPlay", "Touch", "Serve",
			"ReturnSkill", "Speed", "Acceleration", "Agility", "Power", "Reach", "Jump", "Stamina",
			"Anticipation", "Positioning", "Composure", "Consistency", "Teamwork",
		};
		static_assert(sizeof(Names) / sizeof(Names[0]) == static_cast<int>(ERating::Count));
	}
	using namespace PlayerRatingsDetail;
	ERating RatingFromName(std::string_view Name)
	{
		for (int Index = 0; Index < static_cast<int>(ERating::Count); ++Index)
		{
			if (Name == Names[Index])
			{
				return static_cast<ERating>(Index);
			}
		}
		return ERating::Count;
	}

	const char* RatingName(ERating Rating)
	{
		return Rating < ERating::Count ? Names[static_cast<int>(Rating)] : "None";
	}

	int FPlayerRatings::Get(ERating Rating) const
	{
		switch (Rating)
		{
		case ERating::Drive: return Drive;
		case ERating::Backhand: return Backhand;
		case ERating::Volley: return Volley;
		case ERating::Lob: return Lob;
		case ERating::Overhead: return Overhead;
		case ERating::Bandeja: return Bandeja;
		case ERating::Vibora: return Vibora;
		case ERating::WallPlay: return WallPlay;
		case ERating::Touch: return Touch;
		case ERating::Serve: return Serve;
		case ERating::ReturnSkill: return ReturnSkill;
		case ERating::Speed: return Speed;
		case ERating::Acceleration: return Acceleration;
		case ERating::Agility: return Agility;
		case ERating::Power: return Power;
		case ERating::Reach: return Reach;
		case ERating::Jump: return Jump;
		case ERating::Stamina: return Stamina;
		case ERating::Anticipation: return Anticipation;
		case ERating::Positioning: return Positioning;
		case ERating::Composure: return Composure;
		case ERating::Consistency: return Consistency;
		case ERating::Teamwork: return Teamwork;
		default: return 1;
		}
	}

	void FPlayerRatings::Set(ERating Rating, int Value)
	{
		int* Slot = nullptr;
		switch (Rating)
		{
		case ERating::Drive: Slot = &Drive; break;
		case ERating::Backhand: Slot = &Backhand; break;
		case ERating::Volley: Slot = &Volley; break;
		case ERating::Lob: Slot = &Lob; break;
		case ERating::Overhead: Slot = &Overhead; break;
		case ERating::Bandeja: Slot = &Bandeja; break;
		case ERating::Vibora: Slot = &Vibora; break;
		case ERating::WallPlay: Slot = &WallPlay; break;
		case ERating::Touch: Slot = &Touch; break;
		case ERating::Serve: Slot = &Serve; break;
		case ERating::ReturnSkill: Slot = &ReturnSkill; break;
		case ERating::Speed: Slot = &Speed; break;
		case ERating::Acceleration: Slot = &Acceleration; break;
		case ERating::Agility: Slot = &Agility; break;
		case ERating::Power: Slot = &Power; break;
		case ERating::Reach: Slot = &Reach; break;
		case ERating::Jump: Slot = &Jump; break;
		case ERating::Stamina: Slot = &Stamina; break;
		case ERating::Anticipation: Slot = &Anticipation; break;
		case ERating::Positioning: Slot = &Positioning; break;
		case ERating::Composure: Slot = &Composure; break;
		case ERating::Consistency: Slot = &Consistency; break;
		case ERating::Teamwork: Slot = &Teamwork; break;
		default: break;
		}
		if (Slot)
		{
			*Slot = Value < 1 ? 1 : (Value > 99 ? 99 : Value);
		}
	}

	double FPlayerRatings::Unit(ERating Rating, double WrongSideMult) const
	{
		double R = static_cast<double>(Get(Rating)) / 99.0;
		if (IsTechnical(Rating))
		{
			R *= WrongSideMult;
		}
		return R < 0.0 ? 0.0 : (R > 1.0 ? 1.0 : R);
	}
}
