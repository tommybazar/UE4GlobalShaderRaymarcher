# UE4GlobalShaderRaymarcher

A demo showing how a global shader plugin can be used to raymarch a volume texture.

##UPDATE for 4.20
From engine version 4.20, there is full editor support for Volume Textures, so what I'm doing in this plugin can be done way better using the editor + custom material nodes.
 
 Still, the code is useful for learning how to invoke your own shaders with various parameters separated from the standard UE pipeline (and it's not as outdated as most sources available on this topic).

# Installation 
1) Create a new blank C++ project and close it.
2) Copy "Content" and "Plugins" into the root of your new project. Overwrite if asked to do so.
4) Open your project, recompile when asked to do so.

Now when you fire up your project, everything should already be working. Open the map Minimal_Default in StarterContent/Maps and press play.

# Playing around
In the level, you should see a RaymarchVolumeDraw actor. If you scale it, move it around or rotate it, the raymarched volume will be affected accordingly. Notice that the texture will not update or draw when you're in editor, as updates are called from blueprint's Tick().

If you download another RAW file, you can edit the path in the LoadRAWTo3DTexture blueprint, keep in mind it's relative to your project's "Content" folder.

# How it works under the hood

If you know how Raymarching works, you will not be surprised. I use a vertex shader to render a cube with color equal pixel position in object space (if that's not clear to you, make the fragment shader a simple passthrough shader and you'll see what I mean), then give my fragment shader a position that I'm looking from (in model space) and sample the volume texture along the ray connecting these two points.

The interesting part is how this integrates into the rest of Unreal.

Go through the source code in the Raymarcher plugin and see for yourself. Start with BlueprintLibrary codes for how the blueprints connect to the code and then go to RaymarchRendering for the juicy stuff. I commented extensively and tried to keep it clean, so the code should be pretty self-explanatory.

Check out Temaran's (super old) plugin where he's doing something similar and explains more some of the concepts I just skipped.
https://github.com/Temaran/UE4ShaderPluginDemo

# Issues
In "DrawTextureToScreen" widget, I have a hardcoded 1920x1080 resolution for the picture being drawn. Also the "Raymarch_RT" renderTarget texture has 1920x1080 as it's size, change these if your resolution is different.

If you hit "Play" from the editor and your viewport is wider than 16:9, the raymarched texture drawn on the screen will be clipped - just drag your "Content Browser" down to get an aspect ratio of 16:9 or narrower (or play standalone).

# Limitations
Because of the way I render my stuff (completely on the side and then just slap a texture over your view), there is no interaction with other objects whatsoever. The raymarched volume is always on top.

The raymarching pixel shader itself is primitive, but that was not the goal of this exercise. I wanted to rape the global shader plugin to make myself a pipeline aside the regular unreal pipeline (because of missing 3D Texture support). With everything being nicely commented in the code, you can make your own shaders and the appropriate C++ code to set the uniforms and pipeline settings.

# Future work
As EPIC implemented Volume Texture support in editor beginning with version 4.20, what I'm trying to accomplish with this plugin is now doable in a way cleaner, extensible way directly.

Anyhow, as the end-goal is visualizing medical data (which needs a lot of preprocessing), I will probably modify this code to work with the new VolumeTextures for various filtering/modifying the volumes before displaying them. 

An example would be a blueprint node that takes a VolumeTexture and performs a Gaussian Blur on a GPU on it and returns a new filtered texture (because doing that on CPUs takes ages). Other examples are Vesselness filtering, Curvature highlighting etc.  

# Sources

Phillip Rideout - http://prideout.net/blog/?p=64 - I took the box-intersection code from here, also some inspiration for my single-pass shader

IAPR-TC18 - https://www.tc18.org/code_data_set/3D_images.php - I took the aneurism RAW file from here. They have a nice collection of other RAW images.

UE4 documentation on global shaders as plugins - https://docs.unrealengine.com/en-US/Programming/Rendering/ShaderInPlugin/QuickStart - My plugin code started by editing the files provided there, no way I could've figured all this stuff without it.

UE4 Engine source code - this one is a "Thank you" as well as a "Screw you" to EPIC, as I had to figure out a ton of stuff by lurking through the engine, looking at places where the devs were trying to do something similar to what I just needed. Better documentation of the RHI, resources, pipeline initializers and a lot of other low-level stuff would go a long way.

# Special Thanks
Technical University of Munich - Chair for Computer Aided Medical Procedures & Augmented Reality
 
for paying me to play in this awesome engine :)

http://campar.in.tum.de
