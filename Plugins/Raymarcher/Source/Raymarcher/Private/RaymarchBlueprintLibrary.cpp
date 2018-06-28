// Licensed under the WTFPL(Do What the Fuck You Want To Public License) license.

// Loosely based on Epic's own LensDistortion plugin, but changed beyond recognition.

#include "../Public/RaymarchBlueprintLibrary.h"
#include "../Public/RaymarchRendering.h"

#include "UnrealString.h"
#include "Public/Logging/MessageLog.h"
#include <cstdio>
#include "RHICommandList.h"
#include "Classes/Engine/World.h"
#include "Public/GlobalShader.h"
#include "Public/PipelineStateCache.h"
#include "Public/RHIStaticStates.h"
#include "Public/SceneUtils.h"
#include "Public/SceneInterface.h"
#include "Public/ShaderParameterUtils.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

URaymarchBlueprintLibrary::URaymarchBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }

// static
void URaymarchBlueprintLibrary::DrawRaymarchToRenderTarget(
	const UObject* WorldContextObject,
	class UTextureRenderTarget2D* OutputRenderTarget,
	const FTransform Transform)
{
		// Just pass this to Rendering code.
		DrawRaymarchToRenderTarget_GameThread(
		WorldContextObject->GetWorld(), OutputRenderTarget, Transform);
}

void URaymarchBlueprintLibrary::LoadRawTexture3D(const UObject* WorldContextObject, FString textureName, int xDim, int yDim, int zDim)
{
	FIntVector Size(xDim, yDim, zDim);
	const int TotalSize = xDim * yDim * yDim;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	uint8* ByteArray;

	FString RelativePath = FPaths::GameContentDir();
	FString FullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + textureName;

	IFileHandle* FileHandle = PlatformFile.OpenRead(*FullPath);
	if (FileHandle)
	{
		MY_LOG("RAW file was opened!")
		if (FileHandle->Size() < TotalSize) {
			MY_LOG("File is smaller than expected, quitting to avoid segfaults.")
			return;
		}
		else if (FileHandle->Size() > TotalSize) {
			MY_LOG("File is larger than expected, check your dimensions and pixel format (nonfatal, but don't blame me if the texture3d is screwed)")
		}

		// Create our byte buffer and rea the bytes of the file into it.
		ByteArray = reinterpret_cast<uint8*>(FMemory::Malloc(TotalSize));
		FileHandle->Read(ByteArray, TotalSize);
		// Close the file again
		delete FileHandle;
		// Let the whole world know we were successful.
		MY_LOG("File was successfully read!");
	}
	else {
		// Or not.
		MY_LOG("File could not be opened.");
		return;
	}

	// Enqueue
	ENQUEUE_RENDER_COMMAND(CaptureCommand)(
		[ByteArray, Size](FRHICommandListImmediate& RHICmdList)
	{
		Create3DTexture_RenderThread(
			RHICmdList,
			ByteArray,
			Size);
		// Let's not forget to free the memory...
		FMemory::Free(ByteArray);
	}
	);
}

void URaymarchBlueprintLibrary::InitializeRenderResources(class UTextureRenderTarget2D* OutputRenderTarget) {

	check(IsInGameThread());

	if (!OutputRenderTarget)
	{
		MY_LOG("Initializing Render resources witout render target set!")
		return;
	}

	// Set render target clear color to full transparent.
	OutputRenderTarget->ClearColor = FLinearColor(0.0, 0.0, 0.0, 0.0);
	OutputRenderTarget->UpdateResource();

	// Get texture resource to pass to render thread (render thread shall not touch GameThread resources (which OutputRenderTarget is)
	const FName TextureRenderTargetName = OutputRenderTarget->GetFName();
	FTextureRenderTargetResource* TextureRenderTargetResource = OutputRenderTarget->GameThread_GetRenderTargetResource();

	ENQUEUE_RENDER_COMMAND(CaptureCommand)(
		[TextureRenderTargetResource](FRHICommandListImmediate& RHICmdList)
	{
		InitializeResources_RenderThread(RHICmdList, TextureRenderTargetResource);
	}
	);
}

#undef LOCTEXT_NAMESPACE