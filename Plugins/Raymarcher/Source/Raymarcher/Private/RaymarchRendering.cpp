// Licensed under the WTFPL(Do What the Fuck You Want To Public License) license.

#include "../Public/RaymarchRendering.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_SHADER_TYPE(, FRaymarchVS, TEXT("/Plugin/Raymarcher/Private/RaymarchShader.usf"), TEXT("MainVS"), SF_Vertex)
IMPLEMENT_SHADER_TYPE(, FRaymarchPS, TEXT("/Plugin/Raymarcher/Private/RaymarchShader.usf"), TEXT("MainPS"), SF_Pixel)

// Initialize static RenderThreadResources members.
FVector4 RenderThreadResources::CubeVertices[CUBE_VERTEX_CNT] = {};
FIntVector RenderThreadResources::CubeElements[CUBE_TRIANGLE_CNT] = {};
bool RenderThreadResources::initialized = false;
FTexture3DRHIRef RenderThreadResources::VolumeTextureRef = nullptr;
FTexture2DRHIRef RenderThreadResources::DepthTextureRef = nullptr;

// The render thread side of creating a 3d texture. Done on the render thread because it's a render thread resource being set.
void Create3DTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	uint8* RawData,
	FIntVector DataDimensions) {

	check(IsInRenderingThread());

	FRHIResourceCreateInfo CreateInfo;
	RenderThreadResources::VolumeTextureRef = RHICreateTexture3D(
		DataDimensions.X, // Dimension X
		DataDimensions.Y, // Dimension Y
		DataDimensions.Z, // Dimension Z
		PF_G8,		// Pixel format G8 (unsigned byte)
		1,			// Mipmaps (just one)
		TexCreate_ShaderResource, // Flags - don't know if this is necessary, but the flag is there and it works...
		CreateInfo);

	// Size of an U8 (surprisingy equals one, just keeping this here to remember that the strides are in bytes).
	unsigned u8size = 1;
	// Now actually fill it with data. There is also updateBegin/End functions for incremental update, but since we have the whole chunk of
	// data, just fill it in one go.
	const FUpdateTextureRegion3D UpdateRegion(FIntVector::ZeroValue, FIntVector::ZeroValue, DataDimensions);
	RHIUpdateTexture3D(RenderThreadResources::VolumeTextureRef, 0, UpdateRegion, DataDimensions.X * u8size, DataDimensions.X * DataDimensions.Y * u8size, RawData);
}

// Performs actual pipeline settings and rendering commands to draw the raymarched volume.
static void RenderRaymarchToRenderTarget_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	const FCompiledCameraModel& CompiledCameraModel,
	const FName& TextureRenderTargetName,
	FTextureRenderTargetResource* OutTextureRenderTargetResource,
	ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());

	// Set render target to our color texture, depth RT to our depth texture.
	SetRenderTarget(
		RHICmdList,
		OutTextureRenderTargetResource->GetRenderTargetTexture(),	// Color render target - our texture
		RenderThreadResources::DepthTextureRef,						// Depth render target - our created depth texture
		ESimpleRenderTargetMode::EClearColorAndDepth);				// Clear both color and depth

	// Set viewport size.
	RHICmdList.SetViewport(
		0, 0, 0.f, OutTextureRenderTargetResource->GetSizeX(), OutTextureRenderTargetResource->GetSizeY(), 1.f);
	
	// Get shaders.
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef< FRaymarchVS > VertexShader(GlobalShaderMap);
	TShaderMapRef< FRaymarchPS > PixelShader(GlobalShaderMap);

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	// Enable depth-test with NearOrEqual condition.
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
	// Enable blend for colors & alpha, mixed by adding source/dest alpha
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_DestAlpha>::GetRHI();
	// Rasterizer settings - solid fill & culling CW triangles ( CW == only render back-faces -> singlepass raycasting works inside the volume too! )
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	// Rendering a list of triangles.
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	// Declare that vertices are 4 float vectors
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	// Bind shaders to the pipeline we're creating.
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	// Apply all the initializer settings.
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	
	// Update shader uniform parameters. It looks like we're setting all paramaters to both shaders, but unused stuff gets optimized out.
	VertexShader->SetParameters(RHICmdList, VertexShader->GetVertexShader(), CompiledCameraModel);
	PixelShader->SetParameters(RHICmdList, PixelShader->GetPixelShader(), CompiledCameraModel);

	if (RenderThreadResources::VolumeTextureRef) {
		// Set the actual volume texture to the Pixel shader.
		PixelShader->SetTexture(RHICmdList, PixelShader->GetPixelShader(), RenderThreadResources::VolumeTextureRef);
	}
	else {
		// Fallback - If the texture doesn't exist, set black.
		PixelShader->SetTexture(RHICmdList, PixelShader->GetPixelShader(), (FTexture3DRHIParamRef)(FTextureRHIParamRef)GBlackVolumeTexture->TextureRHI);
	}

	// Let the magic happen. 
	DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, CUBE_VERTEX_CNT, CUBE_TRIANGLE_CNT, RenderThreadResources::CubeElements, sizeof(FIntVector), RenderThreadResources::CubeVertices, sizeof(FVector4));

	// Resolve render target.
	RHICmdList.CopyToResolveTarget(
		OutTextureRenderTargetResource->GetRenderTargetTexture(),
		OutTextureRenderTargetResource->TextureRHI,
		false, FResolveParams());
}

// CPU-side of getting uniforms ready for the shader.
void DrawRaymarchToRenderTarget_GameThread(
	UWorld* World,
	UTextureRenderTarget2D* OutputRenderTarget,
	const FTransform Transform)
{
	check(IsInGameThread());

	if (!OutputRenderTarget)
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Trying to render raymarch with render target not set!"));
	}

	// Get texture resource to pass to render thread.
	const FName TextureRenderTargetName = OutputRenderTarget->GetFName();
	FTextureRenderTargetResource* TextureRenderTargetResource = OutputRenderTarget->GameThread_GetRenderTargetResource();
	
	// The following part is a rudiculously complicated way of getting the view and projection matrices of the FirstPlayer camera.
	// To be honest, everything except the CalcSceneView could probably be done beforehand instead of every tick.
	APlayerCameraManager* Manager = World->GetFirstPlayerController()->PlayerCameraManager;
	ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		LocalPlayer->ViewportClient->Viewport,
		World->Scene, 
		LocalPlayer->ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(true));

	FVector ViewLocation;
	FRotator ViewRotation;
	FSceneView* SceneView = LocalPlayer->CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, LocalPlayer->ViewportClient->Viewport);

	// Create a struct with uniforms to pass to shaders on render thread, fill with calculated values.
	FCompiledCameraModel CompiledCameraModel;

	CompiledCameraModel.ModelMatrix = Transform.ToMatrixWithScale();
	CompiledCameraModel.ViewMatrix = SceneView->ViewMatrices.GetViewMatrix();
	CompiledCameraModel.ProjectionMatrix = SceneView->ViewMatrices.GetProjectionMatrix();
	// Need to put RayOrigin to camera position in Model Space
	CompiledCameraModel.RayOrigin = Transform.ToInverseMatrixWithScale().TransformPosition(ViewLocation);
	
	ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

	// Call the actual rendering code on RenderThread.
	ENQUEUE_RENDER_COMMAND(CaptureCommand)(
		[CompiledCameraModel, TextureRenderTargetResource, TextureRenderTargetName, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			RenderRaymarchToRenderTarget_RenderThread(
				RHICmdList,
				CompiledCameraModel,
				TextureRenderTargetName,
				TextureRenderTargetResource,
				FeatureLevel);
		}
	);
}


void InitializeResources_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRenderTargetResource* TextureRenderTargetResource) {
	check(IsInRenderingThread());

	// Cube vertices front
	RenderThreadResources::CubeVertices[0].Set(-1.0, -1.0, 1.0, 1.0);
	RenderThreadResources::CubeVertices[1].Set(1.0, -1.0, 1.0, 1.0);
	RenderThreadResources::CubeVertices[2].Set(1.0, 1.0, 1.0, 1.0);
	RenderThreadResources::CubeVertices[3].Set(-1.0, 1.0, 1.0, 1.0);
	// Cube vertices back
	RenderThreadResources::CubeVertices[4].Set(-1.0, -1.0, -1.0, 1.0);
	RenderThreadResources::CubeVertices[5].Set(1.0, -1.0, -1.0, 1.0);
	RenderThreadResources::CubeVertices[6].Set(1.0, 1.0, -1.0, 1.0);
	RenderThreadResources::CubeVertices[7].Set(-1.0, 1.0, -1.0, 1.0);

	// Triangle indexes.
	// Front
	RenderThreadResources::CubeElements[0] = FIntVector(0, 1, 2);
	RenderThreadResources::CubeElements[1] = FIntVector(2, 3, 0);
	// right
	RenderThreadResources::CubeElements[2] = FIntVector(1, 5, 6);
	RenderThreadResources::CubeElements[3] = FIntVector(6, 2, 1);
	// back
	RenderThreadResources::CubeElements[4] = FIntVector(7, 6, 5);
	RenderThreadResources::CubeElements[5] = FIntVector(5, 4, 7);
	// left
	RenderThreadResources::CubeElements[6] = FIntVector(4, 0, 3);
	RenderThreadResources::CubeElements[7] = FIntVector(3, 7, 4);
	// bottom
	RenderThreadResources::CubeElements[8] = FIntVector(4, 5, 1);
	RenderThreadResources::CubeElements[9] = FIntVector(1, 0, 4);
	// top
	RenderThreadResources::CubeElements[10] = FIntVector(3, 2, 6);
	RenderThreadResources::CubeElements[11] = FIntVector(6, 7, 3);
	// @note - Triangles are clockwise facing outside -> Cull CCW to render front faces.
	
	// Set clear color for the depth render-target texture (DepthFar, so that everything passes the depth-test on a cleared texture).
	FRHIResourceCreateInfo CreateInfo(FClearValueBinding::DepthFar);
	// Create the actual depth texture. Same size as the output render target. Notice format == PF_DepthStencil and flags == TexCreate_DepthStencilTargetable
	RenderThreadResources::DepthTextureRef =
		RHICmdList.CreateTexture2D(	TextureRenderTargetResource->GetSizeX(),
									TextureRenderTargetResource->GetSizeY(),
									PF_DepthStencil,
									1,
									1,
									TexCreate_DepthStencilTargetable,
									CreateInfo);

	RenderThreadResources::initialized = true;
}


#undef LOCTEXT_NAMESPACE
