// Copyright Herbert Mehlhose, Herb64, 2025

#include "LumenSwitchComponentBase.h"
#include "Logging/StructuredLog.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
// #include "Components/SphereComponent.h"
#include "Components/BrushComponent.h"
// #include "Components/PostProcessComponent.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "Components/LineBatchComponent.h"
#include "Curves/CurveLinearColor.h"


DEFINE_LOG_CATEGORY_STATIC(LogLumenSwitcher, Log, All)


ULumenSwitchComponentBase::ULumenSwitchComponentBase()
{
	PrimaryComponentTick.bCanEverTick = true;
}

/**
 * This Actor Component should only be used in Development Builds with Editor. E.g. using GetActorLabel() ...
 * The initial Post Process Component approach has been dropped completely, using Camera PP Settings instead.
 * This solves the issue, that high prio PP component settings did get overridden by
 * any lower prio PP Volume in level, if that volume just has a minimal non zero priority. 
 * I also posted this on Unreal Source - it looks like there's an engine bug.. ???
 * https://discord.com/channels/187217643009212416/375020523240816640/1344239039070605362
 */
void ULumenSwitchComponentBase::BeginPlay()
{
	Super::BeginPlay();

	// We only should attach this to a character with a Camera Component
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		PlayerCameraComponent = OwnerCharacter->FindComponentByClass<UCameraComponent>();
		if (!PlayerCameraComponent)
		{
			UE_LOGFMT(LogLumenSwitcher, Error, "{0}: No camera component found on the Owner Character", __FUNCTION__);
		}
	}
	else
	{
		UE_LOGFMT(LogLumenSwitcher, Error, "{0}: Cannot access owner Character. The component requires a Character Actor", __FUNCTION__);
	}
	
	bIsOVerrideEnabled = bEnableAtStart;
	MaxPPVolPrioInLevel = GetPostProcessVolumesInLevel(PPVolumesInLevel, true);

	SetupEnhancedInput();

	// Setting: Use Hardware Raytracing When Available
	GetDefaultLumen_HardwareRayTracing();    

	FPostProcessSettings CurrentPPSettings;
	GetCurrentPostProcessSettings(CurrentPPSettings);
	PlayerCameraComponent->PostProcessSettings.bOverride_SceneColorTint = false;
	PlayerCameraComponent->PostProcessSettings.bOverride_ReflectionMethod = bIsOVerrideEnabled;
	PlayerCameraComponent->PostProcessSettings.ReflectionMethod = CurrentPPSettings.ReflectionMethod;
	PlayerCameraComponent->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = bIsOVerrideEnabled;
	PlayerCameraComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = CurrentPPSettings.DynamicGlobalIlluminationMethod;

	if (bVisualizePPVolBounds)
	{
		VisualizePostprocessVolumesInLevel(-1.f);
	}
}


void ULumenSwitchComponentBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FPSRefreshRate == 0.f) OnUpdateUI(DeltaTime);
	FrameCount++;
	AccuTime += DeltaTime;
	if (AccuTime >= FPSRefreshRate)
	{
		OnUpdateUI(FrameCount / AccuTime);
		FrameCount = 0;
		AccuTime = 0.0f;
	}
}

bool ULumenSwitchComponentBase::ToggleOverrides()
{
	bIsOVerrideEnabled = !bIsOVerrideEnabled;
	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->PostProcessSettings.bOverride_ReflectionMethod = bIsOVerrideEnabled;
		PlayerCameraComponent->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = bIsOVerrideEnabled;
	}
	return bIsOVerrideEnabled;
}


bool ULumenSwitchComponentBase::IsOverrideEnabled() const
{
	return bIsOVerrideEnabled;
}

/**
 * No more need for enabling collision on ppvols and adding a sphere collision to the camera for functions 
 * like GetOverlappingActors() and IsOverlappingActor(). Inspired by the logic the engine uses internally
 * with EncompassesPoint() function.
 * 1. Use World->PostProcessVolumes existing Array, already sorted in ascending order of priority instead of GetAllActorsOfClass() or TActorIterator as done in my old function
 * 2. Use EncompassesPoint() from Volume.cpp as done in World.cpp DoPostProcessVolume(). Important: handle the false returned for infinite volumes for our special case
 * This takes blend radius into account. No longer having that ugly radius for the camera collision sphere used before which was not really safe in terms of accuracy.
 * This function gets called for each update of the PP Volume information ListBox.
 * @TODO: just use PPVolumesInLevel Map - if already filled, just iterate this one and check encompass function
 */
float ULumenSwitchComponentBase::GetPostProcessVolumesInLevel(TMap<FName, FPostProcessVolumeInfo>& PPVolMap, bool bDebug)
{
	UWorld* World = GetWorld();
	if (!World || !PlayerCameraComponent) return 0.f;
	float Prio = 0.f;
	TArray<IInterface_PostProcessVolume*> PPVolInterfaces = World->PostProcessVolumes;
	for (IInterface_PostProcessVolume* PPVolInterface : PPVolInterfaces)
	{
		float Distance = UE_BIG_NUMBER;
		FPostProcessVolumeProperties Properties = PPVolInterface->GetProperties();
		// Cast UObject to APostProcessVolume is only needed here to get the ActorLabel for the ListView display
		if (APostProcessVolume* PPVol = Cast<APostProcessVolume>(PPVolInterface->_getUObject()))
		{
			FName DisplayName = FName(*PPVol->GetActorLabel());
			FPostProcessVolumeInfo Info = FPostProcessVolumeInfo();
			Info.bIsInfinte = Properties.bIsUnbound;
			Info.Priority = Properties.Priority;
			Info.bIsEnabled = Properties.bIsEnabled;
			Prio = Info.Priority;
			bool bEncompass = PPVolInterface->EncompassesPoint(PlayerCameraComponent->GetComponentLocation(), Properties.BlendRadius, &Distance);
			// For infinite PP Volume - camera is inside the volume, obviously :)
			Info.bCameraEncompassed = bEncompass || Properties.bIsUnbound;
			PPVolMap.Add(DisplayName, Info);
		}
	}
	if (bDebug)
	{
		TArray<FName> Keys;
		PPVolMap.GetKeys(Keys);
		for (FName Key : Keys)
		{
			FPostProcessVolumeInfo* Info = PPVolMap.Find(Key);
			if (Info)
			{
				UE_LOGFMT(LogLumenSwitcher, Display, "{0}: PPVol {1}, Inf={2}, Prio={3}, CamInside={4}", __FUNCTION__, Key, Info->bIsInfinte, Info->Priority, Info->bCameraEncompassed);
			}
		}
	}
	// Prio contains the highest priority due to the sorted nature of World->PostProcessVolumes
	return Prio;
}

/**
 * This function is meant to be called when override of settings is enabled. 
 * This could be helpful to solve the Priority problem with Volumes that have lower priority
 * but greater 0.0f overriding the higher priority components PP settings.
 * @TODO: this will require to keep track of the current state in case we reenable later.
 */
void ULumenSwitchComponentBase::DisableAllPostprocessVolumesInLevel()
{
	UWorld* World = GetWorld();
	TArray<IInterface_PostProcessVolume*> PPVolInterfaces = World->PostProcessVolumes;
	if (!World) return;
	for (IInterface_PostProcessVolume* PPVolInterface : PPVolInterfaces)
	{
		if (APostProcessVolume* PPVol = Cast<APostProcessVolume>(PPVolInterface->_getUObject()))
		{
			PPVol->bEnabled = false;
		}
	}
}


void ULumenSwitchComponentBase::VisualizePostprocessVolumesInLevel(float LifeTime)
{
	if (!bVisualizePPVolBounds) return;
	UWorld* World = GetWorld();
	if (!World) return;
	TArray<IInterface_PostProcessVolume*> PPVolInterfaces = World->PostProcessVolumes;
	for (IInterface_PostProcessVolume* PPVolInterface : PPVolInterfaces)
	{
		if (APostProcessVolume* PPVol = Cast<APostProcessVolume>(PPVolInterface->_getUObject()))
		{
			if (bColorizeByPriority)
			{
				if (!VisualizationColorCurve)
				{
					UE_LOGFMT(LogLumenSwitcher, Error, "{0}: No Visualization Color Curve has been selected for Post Process Volumes... skipping", __FUNCTION__);
					return;
				}
				float RelativePrio = MaxPPVolPrioInLevel < UE_SMALL_NUMBER ? 1.f : PPVol->Priority / MaxPPVolPrioInLevel;
				VisualizePPVol(PPVol, VisualizationColorCurve->GetLinearColorValue(RelativePrio).ToFColor(true), LifeTime, VisualizationLineThickness);
			}
			else
			{
				VisualizePPVol(PPVol, VisualizationColor.ToFColor(true), LifeTime, VisualizationLineThickness);
			}
		}
	}
}


/**
 * Debug Draw for the Post Process Volume bounds as OOB, not just getting coarse bounds. Rotated
 * Volumes are handled correctly. In addition, we also consider the BlendRadius when displaying
 * the Bounds.
 * Note: LineBatcher is your friend, see DrawDebugHelpers.cpp
 * This only is intended to be used for box shaped Post Process Volumes!
 */
void ULumenSwitchComponentBase::VisualizePPVol(APostProcessVolume* PPVol, const FColor& Color, float LifeTime, float Thickness)
{
	UWorld* World = GetWorld();
	if (!World || !PPVol || PPVol->bUnbound) return;

	// Just in case, not sure if this may happen - we only handle standard box shaped PPVols
	UBrushComponent* BrushComp = PPVol->GetBrushComponent();
	if (!BrushComp) return;
	if (BrushComp->Brush->Points.Num() != 8)
	{
		UE_LOGFMT(LogLumenSwitcher, Display, "{0}: PPVol {1} does not have exactly 8 Brush Points, skipping visualization...", __FUNCTION__, PPVol->GetActorLabel());
	}

	const FTransform& ActorToWorld = PPVol->GetTransform();
	FVector Scale = ActorToWorld.GetScale3D();
	float R = PPVol->BlendRadius;
	float X = Scale.X * 100.f;
	float Y = Scale.Y * 100.f;
	float Z = Scale.Z * 100.f;
	float F1 = FMath::Sin(UE_PI / 8.f);
	float F2 = FMath::Sin(UE_PI / 4.f);
	float F3 = FMath::Cos(UE_PI / 8.f);

	// Non Transformed points in local space, ordered for later line segment creation
	TArray<FVector> XPoints;
	XPoints.Empty(20);
	XPoints.Add(FVector(X, -Y - R, -Z));
	XPoints.Add(FVector(X, -Y - R, Z));
	XPoints.Add(FVector(X, -Y - R * F3, Z + R * F1));
	XPoints.Add(FVector(X, -Y - R * F2, Z + R * F2));
	XPoints.Add(FVector(X, -Y - R * F1, Z + R * F3));
	XPoints.Add(FVector(X, -Y, Z + R));
	XPoints.Add(FVector(X, Y, Z + R));
	XPoints.Add(FVector(X, Y + R * F1, Z + R * F3));
	XPoints.Add(FVector(X, Y + R * F2, Z + R * F2));
	XPoints.Add(FVector(X, Y + R * F3, Z + R * F1));
	XPoints.Add(FVector(X, Y + R, Z));
	XPoints.Add(FVector(X, Y + R, -Z));
	XPoints.Add(FVector(X, Y + R * F3, -Z - R * F1));
	XPoints.Add(FVector(X, Y + R * F2, -Z - R * F2));
	XPoints.Add(FVector(X, Y + R * F1, -Z - R * F3));
	XPoints.Add(FVector(X, Y, -Z - R));
	XPoints.Add(FVector(X, -Y, -Z -R));
	XPoints.Add(FVector(X, -Y - R * F1, -Z - R * F3));
	XPoints.Add(FVector(X, -Y - R * F2, -Z - R * F2));
	XPoints.Add(FVector(X, -Y - R * F3, -Z - R * F1));

	TArray<FVector> YPoints;
	YPoints.Empty(20);
	YPoints.Add(FVector(-X - R, Y, -Z));
	YPoints.Add(FVector(-X - R, Y, Z));
	YPoints.Add(FVector(-X - R * F3, Y, Z + R * F1));
	YPoints.Add(FVector(-X - R * F2, Y, Z + R * F2));
	YPoints.Add(FVector(-X - R * F1, Y, Z + R * F3));
	YPoints.Add(FVector(-X, Y, Z + R));
	YPoints.Add(FVector(X, Y, Z + R));
	YPoints.Add(FVector(X + R * F1, Y, Z + R * F3));
	YPoints.Add(FVector(X + R * F2, Y, Z + R * F2));
	YPoints.Add(FVector(X + R * F3, Y, Z + R * F1));
	YPoints.Add(FVector(X + R, Y, Z));
	YPoints.Add(FVector(X + R, Y, -Z));
	YPoints.Add(FVector(X + R * F3, Y, -Z - R * F1));
	YPoints.Add(FVector(X + R * F2, Y, -Z - R * F2));
	YPoints.Add(FVector(X + R * F1, Y, -Z - R * F3));
	YPoints.Add(FVector(X, Y, -Z - R));
	YPoints.Add(FVector(-X, Y, -Z - R));
	YPoints.Add(FVector(-X - R * F1, Y, -Z - R * F3));
	YPoints.Add(FVector(-X - R * F2, Y, -Z - R * F2));
	YPoints.Add(FVector(-X - R * F3, Y, -Z - R * F1));

	TArray<FVector> ZPoints;
	ZPoints.Empty(20);
	ZPoints.Add(FVector(-X, -Y - R, Z));
	ZPoints.Add(FVector(X, -Y - R, Z));
	ZPoints.Add(FVector(X + R * F1, -Y - R * F3, Z));
	ZPoints.Add(FVector(X + R * F2, -Y - R * F2, Z));
	ZPoints.Add(FVector(X + R * F3, -Y - R * F1, Z));
	ZPoints.Add(FVector(X + R, -Y, Z));
	ZPoints.Add(FVector(X + R, Y, Z));
	ZPoints.Add(FVector(X + R * F3, Y + R * F1, Z));
	ZPoints.Add(FVector(X + R * F2, Y + R * F2, Z));
	ZPoints.Add(FVector(X + R * F1, Y + R * F3, Z));
	ZPoints.Add(FVector(X, Y + R, Z));
	ZPoints.Add(FVector(-X, Y + R, Z));
	ZPoints.Add(FVector(-X - R * F1, Y + R * F3, Z));
	ZPoints.Add(FVector(-X - R * F2, Y + R * F2, Z));
	ZPoints.Add(FVector(-X - R * F3, Y + R * F1, Z));
	ZPoints.Add(FVector(-X - R, Y, Z));
	ZPoints.Add(FVector(-X - R, -Y, Z));
	ZPoints.Add(FVector(-X - R * F3, -Y - R * F1, Z));
	ZPoints.Add(FVector(-X - R * F2, -Y - R * F2, Z));
	ZPoints.Add(FVector(-X - R * F1, -Y - R * F3, Z));

	// Arrays for the Transformed Positions, 2 versions for each axis
	TArray<FVector> XPointsA;
	TArray<FVector> XPointsB;
	TArray<FVector> YPointsA;
	TArray<FVector> YPointsB;
	TArray<FVector> ZPointsA;
	TArray<FVector> ZPointsB;
	for (const FVector& Point : XPoints)
	{
		XPointsA.Add(ActorToWorld.TransformPositionNoScale(Point));
		XPointsB.Add(ActorToWorld.TransformPositionNoScale(FVector(-Point.X, Point.Y, Point.Z)));
	}
	for (const FVector& Point : YPoints)
	{
		YPointsA.Add(ActorToWorld.TransformPositionNoScale(Point));
		YPointsB.Add(ActorToWorld.TransformPositionNoScale(FVector(Point.X, -Point.Y, Point.Z)));
	}
	for (const FVector& Point : ZPoints)
	{
		ZPointsA.Add(ActorToWorld.TransformPositionNoScale(Point));
		ZPointsB.Add(ActorToWorld.TransformPositionNoScale(FVector(Point.X, Point.Y, -Point.Z)));
	}

	// Create Batched LineSegments from the Transformed Point Positions
	ULineBatchComponent* LineBatcher = World->PersistentLineBatcher; // LineBatcher / ForegroundLineBatcher
	TArray<FBatchedLine> LineSegments;
	LineSegments.Empty(120);
	int32 Last = XPointsA.Num() - 1;
	for (int32 i = 0; i < Last; i++)
	{
		LineSegments.Add(FBatchedLine(XPointsA[i], XPointsA[i + 1], Color, LifeTime, Thickness, 0));
		LineSegments.Add(FBatchedLine(XPointsB[i], XPointsB[i + 1], Color, LifeTime, Thickness, 0));
		LineSegments.Add(FBatchedLine(YPointsA[i], YPointsA[i + 1], Color, LifeTime, Thickness, 0));
		LineSegments.Add(FBatchedLine(YPointsB[i], YPointsB[i + 1], Color, LifeTime, Thickness, 0));
		LineSegments.Add(FBatchedLine(ZPointsA[i], ZPointsA[i + 1], Color, LifeTime, Thickness, 0));
		LineSegments.Add(FBatchedLine(ZPointsB[i], ZPointsB[i + 1], Color, LifeTime, Thickness, 0));
	}
	LineSegments.Add(FBatchedLine(XPointsA[Last], XPointsA[0], Color, LifeTime, Thickness, 0));
	LineSegments.Add(FBatchedLine(XPointsB[Last], XPointsB[0], Color, LifeTime, Thickness, 0));
	LineSegments.Add(FBatchedLine(YPointsA[Last], YPointsA[0], Color, LifeTime, Thickness, 0));
	LineSegments.Add(FBatchedLine(YPointsB[Last], YPointsB[0], Color, LifeTime, Thickness, 0));
	LineSegments.Add(FBatchedLine(ZPointsA[Last], ZPointsA[0], Color, LifeTime, Thickness, 0));
	LineSegments.Add(FBatchedLine(ZPointsB[Last], ZPointsB[0], Color, LifeTime, Thickness, 0));
	
	LineBatcher->DrawLines(LineSegments);
}


/**
 * TODO - play with this to see, how this can be used in a reproducible way with full
 * control on start and end points of the arc. Could then be part of a BPFL.
 */
void ULumenSwitchComponentBase::DrawDebugArc(const FVector& Center, float Radius, const FVector& Direction, float AngleWidth, int32 Segments, const FColor& Color, bool PersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	UWorld* World = GetWorld();
	if (!World) return;
	DrawDebugCircleArc(World, Center, Radius, Direction, AngleWidth, Segments, Color, PersistentLines, LifeTime, DepthPriority, Thickness);
}


bool ULumenSwitchComponentBase::IsCameraInside(APostProcessVolume* PPVolume) const
{
	if (!PPVolume) return false;
	float Distance = UE_BIG_NUMBER;
	FPostProcessVolumeProperties Properties = PPVolume->GetProperties();
	if (PlayerCameraComponent)
	{
		return PPVolume->EncompassesPoint(PlayerCameraComponent->GetComponentLocation(), Properties.BlendRadius, &Distance);
	}
	else return false;
}


/**
 * Toggle The Global Illumination Method - new version using the Camera PP Settings instead of the PP Component
 * Important: if we change any value, we need to set the bOverride_nnn to true. This is the equivalent to the checkbox in
 * blueprint, using the meta = (PinHiddenByDefault, InlineEditConditionToggle). Any values not being checked
 * are not considered, even if the Priority is high enough.
 * See also the nice Macros SET_PP, LERP_PP and IF_PP in SceneView.cpp
 * Note: we do not consider the "Plugin" method for GI
 */
void ULumenSwitchComponentBase::ToggleGlobalIlluminationMethod()
{
	if (!bIsOVerrideEnabled) return;
	FPostProcessSettings PPSettingsCurrent;
	GetCurrentPostProcessSettings(PPSettingsCurrent);
	PlayerCameraComponent->PostProcessSettings.bOverride_ReflectionMethod = true;
	PlayerCameraComponent->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
	switch (PPSettingsCurrent.DynamicGlobalIlluminationMethod)
	{
	case EDynamicGlobalIlluminationMethod::None:
		PlayerCameraComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		break;
	case EDynamicGlobalIlluminationMethod::Lumen:
		PlayerCameraComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::ScreenSpace;
		break;
	case EDynamicGlobalIlluminationMethod::ScreenSpace:
		PlayerCameraComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		break;
	default:
		break;
	}
}


void ULumenSwitchComponentBase::ToggleReflectionMethod()
{
	if (!bIsOVerrideEnabled) return;
	FPostProcessSettings PPSettingsCurrent;
	GetCurrentPostProcessSettings(PPSettingsCurrent);
	PlayerCameraComponent->PostProcessSettings.bOverride_ReflectionMethod = true;
	PlayerCameraComponent->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
	switch (PPSettingsCurrent.ReflectionMethod)
	{
	case EReflectionMethod::None:
		PlayerCameraComponent->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
		break;
	case EReflectionMethod::Lumen:
		PlayerCameraComponent->PostProcessSettings.ReflectionMethod = EReflectionMethod::ScreenSpace;
		break;
	case EReflectionMethod::ScreenSpace:
		PlayerCameraComponent->PostProcessSettings.ReflectionMethod = EReflectionMethod::None;
		break;
	default:
		break;
	}
}


/**
 * Get the effective Post Process Settings for the current View.
 * Source code for LocalPlayer.cpp and SceneView.cpp - lot of PP Stuff in here for learning.
 * If not starting in Selected Viewport, but New Editor Window PIE - SceneView null gets returned and 
 * causes crash. Simply skipping makes things magically work, although I'd have expected it
 * to simply not return correct pp settings.
 * @TODO - try to understand why...
 */
void ULumenSwitchComponentBase::GetCurrentPostProcessSettings(FPostProcessSettings& OutPPSettings) const
{
	UWorld* World = GetWorld();
	if (World)
	{
		ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
		if (LocalPlayer)
		{
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
				LocalPlayer->ViewportClient->Viewport,
				World->Scene,
				LocalPlayer->ViewportClient->EngineShowFlags)
				.SetRealtimeUpdate(true));
			FVector ViewLocation;
			FRotator ViewRotation;
			FSceneView* SceneView = LocalPlayer->CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, LocalPlayer->ViewportClient->Viewport);
			if (SceneView)
			{
				OutPPSettings = SceneView->FinalPostProcessSettings;
			}
			else
			{
				UE_LOGFMT(LogLumenSwitcher, Error, "{0}: no valid Sceneview for LocalPlayer {1}", __FUNCTION__, LocalPlayer->GetName());
			}
		}
	}
}


/**
 * This is actually not really used currently...
 * @TODO: maybe things could be done using the Camera PostProcess instead of having a component. Tests did fail, but should revisit this
 */
void ULumenSwitchComponentBase::GetCameraPostProcessSettings(FPostProcessSettings& OutPPSettings) const
{
	if (PlayerCameraComponent)
	{
		OutPPSettings = PlayerCameraComponent->PostProcessSettings;
	}
}


void ULumenSwitchComponentBase::SetupEnhancedInput()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(SwitcherInputMappingContext, 0);
		}
	}
}


#pragma region ProjectSettings_Related

bool ULumenSwitchComponentBase::GetDefaultLumen_HardwareRayTracing()
{
	GConfig->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Lumen.HardwareRayTracing"), bLumenUseHardwareRayTracing, GEngineIni);
	return bLumenUseHardwareRayTracing;
}

bool ULumenSwitchComponentBase::GetCurrentLumen_HardwareRayTracing()
{
	return bLumenUseHardwareRayTracing;
}

bool ULumenSwitchComponentBase::ToggleLumenHardwareRayTracing()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	bLumenUseHardwareRayTracing = !bLumenUseHardwareRayTracing;
	PC->ConsoleCommand(FString::Printf(TEXT("r.Lumen.HardwareRayTracing %d"), bLumenUseHardwareRayTracing), true);
	return bLumenUseHardwareRayTracing;
}

#pragma endregion ProjectSettings_Related

/**
 * Add PostProcess Component to the owner Character
 * Note: a PostProcessComponent is always unbound, unless it's directly attached to a ShapeComponent like a BoxComponent or SphereComponent.
 */
 /*void ULumenSwitchComponentBase::AddPostProcessComponentToOwnerCharacter(float Priority)
 {
	 bool bSuccess = false;
	 AActor* OwnerActor = GetOwner();
	 if (OwnerActor)
	 {
		 FTransform xForm;
		 PostProcessComponent = Cast<UPostProcessComponent>(OwnerActor->AddComponentByClass(UPostProcessComponent::StaticClass(), false, xForm, false));
		 if (PostProcessComponent)
		 {
			 PostProcessComponent->Priority = Priority;
			 PostProcessComponent->bUnbound = true;
			 PostProcessComponent->Rename(TEXT("PPComponent"));
			 bSuccess = PostProcessComponent->AttachToComponent(OwnerActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		 }
	 }
 }*/
