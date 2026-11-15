#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UI/Model/PadelUiEventSink.h"

class UPadelMatchEvents;

namespace Padel::Ui
{
	class FCoreEventAdapter;
}

/**
 * Binds the UI to UPadelMatchEvents (GDD 3.4): every delegate is forwarded into an
 * IUiEventSink through FCoreEventAdapter, so the codes and the score view are made in the
 * tested, engine-free model. World centimetres go back to metres here, the one place the
 * UI crosses that boundary (CLAUDE.md, "Coordinates and units").
 */
class PADEL_API FPadelMatchEventBinder
{
public:
	FPadelMatchEventBinder();
	~FPadelMatchEventBinder();

	FPadelMatchEventBinder(const FPadelMatchEventBinder&) = delete;
	FPadelMatchEventBinder& operator=(const FPadelMatchEventBinder&) = delete;

	/** Unbinds from any earlier subsystem first. */
	void Bind(UPadelMatchEvents& Events, Padel::Ui::IUiEventSink& Sink, bool bGoldenPointRule);
	void Unbind();

	/** The score view's golden point flag is the setup's rule; it changes per match. */
	void SetGoldenPointRule(bool bOn);

	bool IsBound() const { return Events.IsValid(); }

	/** True when a real match simulation is attached to the bound subsystem. */
	bool HasLiveMatch() const;

private:
	TWeakObjectPtr<UPadelMatchEvents> Events;
	TUniquePtr<Padel::Ui::FCoreEventAdapter> Adapter;
	TArray<FDelegateHandle> Handles;
};
