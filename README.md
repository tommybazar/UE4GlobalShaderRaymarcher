# UE4GlobalShaderRaymarcher

A demo showing how a global shader plugin can be used to raymarch a volume texture.

# Installation 
1) Fire up your UE4.sln project from Visual studio and create an empty c++ project and close it.
2) Copy "Raymarcher" directory from "Plugins" into your engine plugins folder (you must have source version of Unreal - packaging a plugin for a binary version seemed like too much of a hassle for now). Regenerate the project files for your solution to include the new plugin.
3) Copy "Content" from "Project" directory over the folder with the same name in your new project and reopen the project.
4) Reopen your UE4.sln and fire it up again. The plugin should compile and be activated by default. Note that It's only currently tested for 4.19.2

Now when you fire up your project, everything should already be working. Open the map Minimal_Default in StarterContent/Maps and press play.

# Playing around
In the level, you should see a RaymarchVolumeDraw actor. If you scale it, move it around or rotate it, the raymarched volume will be affected accordingly. Notice that the texture will not update or draw when you're in editor, as these are called from blueprint's Tick().

If you download another RAW file, you can edit the path in the LoadRAWTo3DTexture blueprint, keep in mind it's relative to your project's "Content" folder.

# How it works under the hood

This section I will finish later, but if you know how Raymarching works, you will not be surprised. I use a vertex shader to render a cube with color == pixel position in object space, then give my fragment shader a position that I'm looking from (in model space) and sample a 3d volume along the ray connecting those two.

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
I am currently working on capturing the scene depth before doing my own rendering, which would allow me to actually place the volume within the scene properly. 
Also, I will probably switch to 2-pass rendering, as having both entry and exit points for raymarch would give me more flexibility with volume cropping.

After that, I want to try integrating 3D texture sampling into a new shader model that would be part of the usual deffered pipeline. This will have some advantages and some disadvantages. For example,
it will need changes in engine source, so I will have to fork the whole engine. Also it will probably be a bit more work. But obviously, this would be a cleaner solution than doing my own rendering on the side and then slapping a texture over the whole scene.

# Sources

Phillip Rideout - http://prideout.net/blog/?p=64 - I took the box-intersection code from here, also some inspiration for my single-pass shader

IAPR-TC18 - https://www.tc18.org/code_data_set/3D_images.php - I took the aneurism raw file from here. They have a nice collection of other raw images.

UE4 documentation on global shaders as plugins - https://docs.unrealengine.com/en-US/Programming/Rendering/ShaderInPlugin/QuickStart - My plugin code started by editing the files provided there, no way I could've figured all this stuff without it.

UE4 Engine source code - this one is a "Thank you" as well as a "Screw you" to Epic, as I had to figure out a ton of stuff by lurking through the engine, looking at places where the devs were trying to do something similar as what I just needed. Better documentation of the RHI, resources, pipeline initializers and a lot of other stuff would go a long way.

# Special Thanks
Technical University of Munich - Chair for Computer Aided Medical Procedures & Augmented Reality
 
 for paying me to play in this awesome engine :)

http://campar.in.tum.de
