// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine.h"
#include "Classes/Engine/TextureRenderTarget2D.h"
#include "Classes/Engine/World.h"
#include "Public/GlobalShader.h"
#include "Public/PipelineStateCache.h"
#include "Public/RHIStaticStates.h"
#include "Public/SceneUtils.h"
#include "Public/SceneInterface.h"
#include "Public/ShaderParameterUtils.h"
#include "Public/Logging/MessageLog.h"
#include "Public/Internationalization/Internationalization.h"

// Shouldn't surprise anyone...
#define CUBE_VERTEX_CNT 8
// 2 triangles per side
#define CUBE_TRIANGLE_CNT 12

#define MY_LOG(x) if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT(x));}

/**
* Camera data to be handled to renderthread, which will in turn set them as uniform shader parameters.
*/
struct FCompiledCameraModel
{
	FMatrix ModelMatrix;
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FVector RayOrigin;
};

// Common ancestor for both Pixel and Vertex shaders.
class FRaymarchShader : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	FRaymarchShader() {}

	FRaymarchShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Texture uniforms
		MyTexture.Bind(Initializer.ParameterMap, TEXT("MyTexture"));
		MySampler.Bind(Initializer.ParameterMap, TEXT("MySampler"));
		// Camera uniforms
		Model.Bind(Initializer.ParameterMap, TEXT("Model"));
		View.Bind(Initializer.ParameterMap, TEXT("View"));
		Projection.Bind(Initializer.ParameterMap, TEXT("Projection"));
		RayOrigin.Bind(Initializer.ParameterMap, TEXT("RayOrigin"));
	}

	template<typename TShaderRHIParamRef>
	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		const TShaderRHIParamRef ShaderRHI,
		const FCompiledCameraModel& CompiledCameraModel)
	{
		// Actually sets the shader uniforms in the pipeline.
		SetShaderValue(RHICmdList, ShaderRHI, Model, CompiledCameraModel.ModelMatrix);
		SetShaderValue(RHICmdList, ShaderRHI, View, CompiledCameraModel.ViewMatrix);
		SetShaderValue(RHICmdList, ShaderRHI, Projection, CompiledCameraModel.ProjectionMatrix);
		SetShaderValue(RHICmdList, ShaderRHI, RayOrigin, CompiledCameraModel.RayOrigin);
	}

	template<typename TShaderRHIParamRef>
	void SetTexture(
			FRHICommandListImmediate& RHICmdList,
			const TShaderRHIParamRef ShaderRHI,
			const FTexture3DRHIRef Texture
		)
	{
		// Create a static sampler reference and bind it together with the texture.
		FSamplerStateRHIParamRef SamplerRef = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		SetTextureParameter(RHICmdList, ShaderRHI, MyTexture, MySampler, SamplerRef, Texture);
	}

	virtual bool Serialize(FArchive& Ar) override
	{			
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MyTexture << MySampler << Model << View << Projection << RayOrigin;
		return bShaderHasOutdatedParameters;
	}

private:
	// Texture resource parameters
	FShaderResourceParameter MyTexture;
	FShaderResourceParameter MySampler;
	// Camera parameters
	FShaderParameter Model;
	FShaderParameter View;
	FShaderParameter Projection;

	FShaderParameter RayOrigin;
};

// Vertex shader class
class FRaymarchVS : public FRaymarchShader
{
	DECLARE_SHADER_TYPE(FRaymarchVS, Global);

public:

	/** Default constructor. */
	FRaymarchVS() {}

	/** Initialization constructor. */
	FRaymarchVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FRaymarchShader(Initializer)
	{
	}
};

// Pixel shader class
class FRaymarchPS : public FRaymarchShader
{
	DECLARE_SHADER_TYPE(FRaymarchPS, Global);

public:

	/** Default constructor. */
	FRaymarchPS() {}

	/** Initialization constructor. */
	FRaymarchPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FRaymarchShader(Initializer)
	{ }
};


/** Prepares uniforms for drawing to render target and then calls render-thread function that performs the rendering.
* @param World Current world to get the rendering settings from (such as feature level).
* @param OutputRenderTarget The render target to draw to.
*/
void DrawRaymarchToRenderTarget_GameThread(
	class UWorld* World,
	class UTextureRenderTarget2D* OutputRenderTarget,
	const FTransform Transform);

/** Creates a 3D texture from given data and stores it in RenderThreadResources. Render thread function!
* @param RawData Byte array of u8 values.
* @param RawDataDimensions 3D Int vector specifying dimensions of the texture.
* @note Only supports UINT8 textures as of now (support for any other kind is easily implemented, though)
*/
void Create3DTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	uint8* RawData,
	FIntVector RawDataDimensions);

/** Initializes resources used by rendering thread 
* @param World Current world to get the rendering settings from (such as feature level).
* @param OutputRenderTarget The render target to draw to. Only need this to take it's resolution for depth-texture creation.
*/
void InitializeResources_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FTextureRenderTargetResource* TextureRenderTargetResource);

// Class for global shader resources - shall only be accessed from render thread!
class RenderThreadResources {
public:
	// Actual loaded volume texture.
	static FTexture3DRHIRef VolumeTextureRef;
	// Depth texture (needed as a separate render target for depth-testing).
	static FTexture2DRHIRef DepthTextureRef;
	// Vertices of a cube used to draw entry points of our raycasted volume.
	static FVector4 CubeVertices[CUBE_VERTEX_CNT];
	// Triangles composed of the vertices.
	static FIntVector CubeElements[CUBE_TRIANGLE_CNT];
	// True if all above data on render thread is initialized.
	static bool initialized;
};