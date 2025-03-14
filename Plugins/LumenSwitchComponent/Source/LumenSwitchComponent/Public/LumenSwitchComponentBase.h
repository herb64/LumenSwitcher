// Copyright Herbert Mehlhose, Herb64, 2025

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LumenSwitchComponentBase.generated.h"

/**
 * Remarks:
 * 1. Need category specifiers for ALL blueprint exposed UPROPERTY and blueprint accessible UFUNCTION statements
 * 2. This Plugin is only meant for Development builds. For example, we use Actor::GetActorLabel()
 * 3. I decided to limit this to Win64, the typical development platform.
 * 4. The original approach attaching a Post Process Component to the player Character has been abandoned. It did
 *    basically work, but unfortunately, even with the component having a higher priority than any pp volume in
 *    the level, it did get overridden by any pp volume with a priority even slightly above 0. Dropped that
 *    component completely and doing all override logic using the Camera PP Settings.
 * 5. In the very beginning, handling PP Volumes had been done by using a camera collision sphere along with
 *    collisions on the PP Volumes - but the "Encompasses" functions are the real way to go
 */

class APostProcessVolume;
//class UPostProcessComponent;
class UInputMappingContext;
//class USphereComponent;
class UCameraComponent;
class UCurveLinearColor;


/** Infos for Post Process Volumes in Level */
USTRUCT(BlueprintType)
struct FPostProcessVolumeInfo
{
	GENERATED_BODY()

	/** Post Process Volume enabled status */
	UPROPERTY(BlueprintReadOnly, Category = "Switcher")
	bool bIsEnabled = true;

	/** Post Process Volume unbound (infinite)? */
	UPROPERTY(BlueprintReadOnly, Category = "Switcher")
	bool bIsInfinte = false;

	/** Post Process Volume Priority */
	UPROPERTY(BlueprintReadOnly, Category = "Switcher")
	float Priority = 0.f;

	/** Is the Camera inside the PP Volume? Always true for infinite PP Volumes */
	UPROPERTY(BlueprintReadOnly, Category = "Switcher")
	bool bCameraEncompassed = false;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable )
class LUMENSWITCHCOMPONENT_API ULumenSwitchComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:	

	ULumenSwitchComponentBase();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;

	/** 
	 * Toggle the Override of Post Process settings 
	 * @return The new status after toggle
	 */
	UFUNCTION(BlueprintCallable, Category = "Switcher", meta = (ReturnDisplayName = "NewOverrideStatus"))
	bool ToggleOverrides();

	/** Is the PostProcess override enabled */
	UFUNCTION(BlueprintCallable, Category = "Switcher", meta = (ReturnDisplayName = "OverrideEnabled"))
	bool IsOverrideEnabled() const;

	/** Get the Value for Lumen "Use Hardware Ray Tracing When Available" from Project settings */
	UFUNCTION(BlueprintCallable, Category = "Switcher")
	bool GetDefaultLumen_HardwareRayTracing();

	/** Get the Value for Lumen Use Hardware Ray Tracing from current setting */
	UFUNCTION(BlueprintCallable, Category = "Switcher", meta = (ReturnDisplayName = "Status"))
	bool GetCurrentLumen_HardwareRayTracing();

	/** Toggle the Value for Lumen Use Hardware Ray Tracing if available */
	UFUNCTION(BlueprintCallable, Category = "Switcher", meta = (ReturnDisplayName = "UseHWRaytracing"))
	bool ToggleLumenHardwareRayTracing();

	/** Get the effective Post Process Settings for the current View */
	UFUNCTION(BlueprintCallable, Category = "Switcher")
	void GetCurrentPostProcessSettings(FPostProcessSettings& OutPPSettings) const;

	/** Get the owning Actors Camera Post Process Settings */
	UFUNCTION(BlueprintCallable, Category = "Switcher")
	void GetCameraPostProcessSettings(FPostProcessSettings& CameraPPSettings) const;

	/** Cycle through available GI Methods (not considering "Plugin" method) */
	UFUNCTION(BlueprintCallable, Category = "Switcher")
	void ToggleGlobalIlluminationMethod();

	/** Cycle through available Reflection Methods */
	UFUNCTION(BlueprintCallable, Category = "Switcher")
	void ToggleReflectionMethod();

	/** 
	 * Get the Post Process Volumes present in the level 
	 * @param	PPVolMap		The Map of PostProcess Volumes
	 * @param	bDebug			Should results be written to log?
	 * @return	float value with highest priority found in all PP Volumes
	 */
	UFUNCTION(BlueprintCallable, Category = "Switcher", meta = (ReturnDisplayName = "MaxPriority"))
	float GetPostProcessVolumesInLevel(TMap<FName, FPostProcessVolumeInfo>& PPVolMap, bool bDebug = false);

	/** Check if Camera is inside a given PP Volume */
	UFUNCTION(BlueprintCallable, Category = "Switcher")
	bool IsCameraInside(APostProcessVolume* PPVolume) const;

	/** Disable all PP Volumes in level */
	UFUNCTION(BlueprintCallable, Category = "Switcher")
	void DisableAllPostprocessVolumesInLevel();

	/** Just kept for further experiments - expose DrawDebugCircleArc() to BluePrint */
	UFUNCTION(BlueprintCallable, Category = "Switcher", meta = (DeprecatedFunction, DeprecationMessage = "Just for experiment, to be removed!"))
	void DrawDebugArc(const FVector& Center, float Radius, const FVector& Direction, float AngleWidth, int32 Segments, const FColor& Color, bool PersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f);

	/** The Post Process Component getting added to the Component Owner Actor */
	//UPROPERTY(BlueprintReadOnly, Category = "Switcher", meta = (DeprecatedProperty, DeprecationMessage = "PostProcess Component no longer used, only working with Camera PP Settings"))
	//TObjectPtr<UPostProcessComponent> PostProcessComponent;

	/** Input Mapping Context for the Switcher Component */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switcher")
	TObjectPtr<UInputMappingContext> SwitcherInputMappingContext;

	/** Should the Post Process Override be enabled by default at BeginPlay? */
	UPROPERTY(EditDefaultsOnly, Category = "Switcher", meta = (DisplayName = "Start with Override enabled"))
	bool bEnableAtStart = true;

	/** User Info Display Widget Update Interval */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Switcher",
		meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "2.0", UIMax = "1.0"),
		meta = (Units = "Seconds", Delta = 0.1f))
	float FPSRefreshRate = 0.3;

	/** Should we Visualize the Post Process Volumes found in Level? */
	UPROPERTY(EditDefaultsOnly, Category = "Switcher|Post Process Volumes")
	bool bVisualizePPVolBounds = false;

	/** The Thickness of the Post Process Volumes Visualization Lines */
	UPROPERTY(EditDefaultsOnly, Category = "Switcher|Post Process Volumes", 
		meta = (EditCondition = "bVisualizePPVolBounds", EditConditionHides,
		ClampMin = "0.0", UIMin = "0.0", ClampMax = "8.0", UIMax = "4.0", Delta = 0.1f))
	float VisualizationLineThickness = 1.f;

	/** Should Color reflect the Post Process Volume Priority? */
	UPROPERTY(EditDefaultsOnly, Category = "Switcher|Post Process Volumes", 
		meta = (EditCondition = "bVisualizePPVolBounds", EditConditionHides))
	bool bColorizeByPriority = false;

	/** Single Fixed Color for Post Process Volumes Visualization */
	UPROPERTY(EditDefaultsOnly, Category = "Switcher|Post Process Volumes", 
		meta = (EditCondition = "bVisualizePPVolBounds && !bColorizeByPriority", EditConditionHides))
	FLinearColor VisualizationColor = FLinearColor(0.f, 1.f, 0.f);

	/** Color curve for Post Process Volumes Visualization with Priority based coloring */
	UPROPERTY(EditAnywhere, Category = "Switcher|Post Process Volumes", 
		meta = (EditCondition = "bVisualizePPVolBounds && bColorizeByPriority", EditConditionHides))
	TObjectPtr<UCurveLinearColor> VisualizationColorCurve;

	/** Update the UI */
	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateUI(float FPS);

private:

	bool bLumenUseHardwareRayTracing = false;
	int32 ReflectionCaptureResolution = 128;
	int32 FrameCount = 0;
	float AccuTime = 0;
	bool bIsOVerrideEnabled = false;
	float MaxPPVolPrioInLevel = 0.f;

	UPROPERTY()
	TObjectPtr<UCameraComponent> PlayerCameraComponent;

	UPROPERTY()
	TMap<FName, FPostProcessVolumeInfo> PPVolumesInLevel;

	void SetupEnhancedInput();
	void VisualizePostprocessVolumesInLevel(float LifeTime = -1.f);
	void VisualizePPVol(APostProcessVolume* PPVol, const FColor& Color, float LifeTime = -1.f, float Thickness = 0.f);
	//void AddPostProcessComponentToOwnerCharacter(float Priority);
};
