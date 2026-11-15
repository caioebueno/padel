#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "PadelInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/** The match input actions (GDD section 7). Names map to IA_<Name>. */
UENUM(BlueprintType)
enum class EPadelInputAction : uint8
{
	Move,
	Sprint,
	ShotDrive,
	ShotLob,
	ShotOverhead,
	/** Post-slice, bound now so the layout is stable. */
	ShotDrop,
	ModAttack,
	ModTouch,
	CmdNet,
	CmdBack,
	CmdMiddle,
	CmdYours,
	Pause,
};

/** How a key feeds a 2D action. Up on the stick, or W, is +Y ("toward the opponents"). */
enum class EPadelAxisMapping : uint8
{
	None,       // digital action, or a 2D key that already reports X/Y (the stick)
	PositiveX,  // D
	NegativeX,  // A
	PositiveY,  // W
	NegativeY,  // S
};

/** One row of the binding table. */
struct PADEL_API FPadelInputBinding
{
	EPadelInputAction Action;
	FKey Key;
	EPadelAxisMapping Axis = EPadelAxisMapping::None;
};

/**
 * Builds the Enhanced Input actions (IA_*) and the IMC_Match mapping context in C++
 * from the GDD 7 table, so bindings are reviewable text rather than binary assets.
 * Gamepad first: every action has a gamepad binding and a keyboard one.
 *
 * Usage: create one per local player (APadelPlayerController does), call Build(),
 * add GetMatchContext() to the Enhanced Input subsystem, bind GetAction(...).
 */
UCLASS()
class PADEL_API UPadelInputConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Creates the actions and the mapping context. Safe to call once. */
	void Build();

	UInputAction* GetAction(EPadelInputAction Action) const;
	UInputMappingContext* GetMatchContext() const { return MatchContext; }

	/** The default binding table (GDD 7). */
	static const TArray<FPadelInputBinding>& GetDefaultBindings();

	/** Every action in declaration order. */
	static TArray<EPadelInputAction> GetAllActions();

	/** "IA_Move", "IA_ShotDrive", ... */
	static FName GetActionAssetName(EPadelInputAction Action);

	static const FName MatchContextName;

	/** Stick dead zone. Rebinding and dead-zone settings are post-slice (GDD 7). */
	static constexpr float StickDeadZone = 0.2f;

private:
	UPROPERTY()
	TMap<EPadelInputAction, TObjectPtr<UInputAction>> Actions;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MatchContext;
};
