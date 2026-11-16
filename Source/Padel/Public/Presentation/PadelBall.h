#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Presentation/Model/PadelBallVisual.h"
#include "VecMath.h"
#include "PadelBall.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * The ball the players and the camera see (GDD 2, GDD 10; docs/decisions/0007-ball.md).
 * The match and practice directors spawn one and call Present() with the simulation's
 * ball every frame; it never feeds anything back to the simulation.
 *
 * It shows the imported SM_Ball (felt, seam, printed mark, fuzz shell at LOD0) turning
 * with the simulation's spin, and SM_BallTrail stretched back along the path at high
 * speed. Spin, trail and teleport handling live in the engine-free FBallVisual, tuned
 * by Data/Ball.json. Until the pipeline has imported SM_Ball it falls back to the engine
 * sphere at 6.5 cm and logs once, so a fresh checkout still plays.
 *
 * A Blueprint subclass may swap the meshes (CLAUDE.md: thin Blueprints pick assets);
 * the logic stays here.
 */
UCLASS()
class PADEL_API APadelBall : public AActor
{
	GENERATED_BODY()

public:
	APadelBall();

	virtual void PostInitializeComponents() override;

	/**
	 * Shows the ball at the simulation's state, in simulation units (metres, m/s, rad/s);
	 * converts to Unreal units here. Frame time comes from the world, so calling it more
	 * than once in a frame only moves the ball. bVisible false hides the ball and its trail.
	 */
	void Present(const Padel::FVec3& PositionM, const Padel::FVec3& VelocityMps, const Padel::FVec3& SpinRadPerS, bool bVisible);

	/** Hides the ball and its trail; the next visible Present starts a new trail. */
	void Hide();

	/** Reads Data/Ball.json ("Visual"); defaults if it is missing or malformed. */
	static Padel::Presentation::FBallVisualSettings LoadSettings();

	UStaticMeshComponent* GetBallMesh() const { return BallMesh; }
	UStaticMeshComponent* GetTrailMesh() const { return TrailMesh; }
	const Padel::Presentation::FBallVisual& GetVisual() const { return Visual; }
	/** True when the imported SM_Ball is shown, false on the engine-sphere fallback. */
	bool IsShowingImportedBall() const { return bImportedBall; }

	/** The imported ball and trail. Soft references, so packaging cooks them. */
	UPROPERTY(EditAnywhere, Category = "Ball")
	TSoftObjectPtr<UStaticMesh> BallAsset;

	UPROPERTY(EditAnywhere, Category = "Ball")
	TSoftObjectPtr<UStaticMesh> TrailAsset;

private:
	void LoadMeshes();
	void ApplyTrail();

	UPROPERTY(VisibleAnywhere, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	UPROPERTY(VisibleAnywhere, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> TrailMesh;

	Padel::Presentation::FBallVisual Visual;
	double LastPresentTime = -1.0;
	bool bShown = false;
	bool bImportedBall = false;
	bool bHasTrail = false;
};
