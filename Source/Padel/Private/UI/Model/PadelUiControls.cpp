#include "UI/Model/PadelUiControls.h"

#include <algorithm>

namespace Padel::Ui
{
	namespace
	{
		int ControlsHintRank(EUiInput Input)
		{
			switch (Input)
			{
			case EUiInput::Confirm:
				return 0;
			case EUiInput::Secondary:
				return 1;
			case EUiInput::Tertiary:
				return 2;
			case EUiInput::TabLeft:
			case EUiInput::TabRight:
				return 3;
			case EUiInput::Pause:
				return 4;
			case EUiInput::Back:
				return 5;
			default:
				return 6;
			}
		}

		bool ControlsStepClamped(int& Index, int Count, int Direction)
		{
			const int Next = Index + (Direction < 0 ? -1 : 1);
			if (Count <= 0 || Next < 0 || Next >= Count)
			{
				return false;
			}
			Index = Next;
			return true;
		}
	}

	bool FSelectorModel::Step(int Direction)
	{
		return ControlsStepClamped(Index, Count, Direction);
	}

	void FSelectorModel::StepForwardWrapping()
	{
		if (Count > 0)
		{
			Index = (Index + 1) % Count;
		}
	}

	bool FToggleModel::Push(int Direction)
	{
		const bool bWant = Direction > 0;
		if (bOn == bWant)
		{
			return false;
		}
		bOn = bWant;
		return true;
	}

	bool FSegmentedModel::Step(int Direction)
	{
		return ControlsStepClamped(Index, Count, Direction);
	}

	bool FSliderModel::Press(int Direction)
	{
		const int Before = Value;
		Set(Value + (Direction < 0 ? -StepSize : StepSize));
		return Value != Before;
	}

	bool FSliderModel::Repeat(int Direction, float HeldSeconds)
	{
		const int Before = Value;
		const int Size = HeldSeconds >= FastAfter ? StepSize : FineStep;
		Set(Value + (Direction < 0 ? -Size : Size));
		return Value != Before;
	}

	void FSliderModel::Set(int NewValue)
	{
		Value = std::clamp(NewValue, Min, Max);
	}

	float FSliderModel::Fraction() const
	{
		return Max > Min ? static_cast<float>(Value - Min) / static_cast<float>(Max - Min) : 0.0f;
	}

	bool FTabModel::Step(int Direction)
	{
		return ControlsStepClamped(Index, Count, Direction);
	}

	std::vector<FHint> OrderHints(const std::vector<FHint>& Hints)
	{
		std::vector<FHint> Ordered;
		bool bHaveTabs = false;
		for (const FHint& Hint : Hints)
		{
			if (Hint.Input == EUiInput::TabLeft || Hint.Input == EUiInput::TabRight)
			{
				if (bHaveTabs)
				{
					continue;
				}
				bHaveTabs = true;
				Ordered.push_back(FHint{EUiInput::TabLeft, Hint.Label});
				continue;
			}
			Ordered.push_back(Hint);
		}
		std::stable_sort(Ordered.begin(), Ordered.end(),
			[](const FHint& A, const FHint& B) { return ControlsHintRank(A.Input) < ControlsHintRank(B.Input); });
		return Ordered;
	}
}
