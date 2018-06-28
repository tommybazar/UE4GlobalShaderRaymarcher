// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Classes/Kismet/BlueprintFunctionLibrary.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RaymarchBlueprintLibrary.generated.h"

UCLASS(MinimalAPI)
class URaymarchBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	/** Draws the raymarched volume .
	 * @param OutputRenderTarget The render target to draw to. Don't necessarily need to have same resolution or aspect ratio as distorted render.
	 */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
	static void DrawRaymarchToRenderTarget(
		const UObject* WorldContextObject,
		class UTextureRenderTarget2D* OutputRenderTarget,
		const FTransform Transform);
	
	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void LoadRawTexture3D(
			const UObject* WorldContextObject,
			FString textureName, int xDim, int yDim, int zDim);

	/** Initializes resources for rendering. Only needs the renderTarget to create a matching DepthTexture. */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher")
		static void InitializeRenderResources(class UTextureRenderTarget2D* OutputRenderTarget);

};