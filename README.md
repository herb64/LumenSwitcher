# Switching Lumen in PIE

Using Lumen or not - lot of discussions going on on about Lumen and the impact on Performance. While Lumen looks great, it has its cost.

This Little Plugin came from the idea, that I wanted to be able to switch Lumen on and off while being in a PIE session to compare visual impact, also regarding reflections. While it started as a simple quick and dirty project, I finally ended up to bring this into more shape and hope, that it can be useful to someone and maybe improved and extended.

## Using the plugin

The plugin is for UE5.5. You can get the complete example project from here and compile yourself.

I added a compiled plugin version for download here: https://www.dropbox.com/scl/fi/fsm9b6y5zx3jloc9qyt0p/LumenSwitchComponent-V1.0-2024-03-14.zip?rlkey=8aq02avtxlx0tbt4b73eoc8hg&dl=0. Simply unpack into your own project into the Plugins folder and add the Component to your Player Character.

The Plugin delivers a simple Actor Component, that simply can be added to the Player Character. Important: only use the **BP_SwitcherComponent**, do not attach the *LumenSwitchComponentBase* to the Character.

It displays a simple Widget with the currently effective Settings active at BeginPlay, based on the camera position, the existing PP Volumes in the level and the Config settings.
The initial Settings are the base, now we can enable/disable the override for the *GI Method* and *Reflection Method* at any point and toggle settings.

We can switch **GI Method** and **Reflection Method** with simple key presses and watch the effect immediately if override is enabled.

**Use Hardware RayTracing if available** can be toggled the same way. This one is not a Post Process configured setting, and it can be toggled independently from the override status. Feel free to adjust the IMC to change keys.

Lumen Method settings are part of *Post Process Settings*, so dealing with Post Process Volumes in the level and camera post process settings is important.

In the end, this did lead to kind of a *Post Process Volume Visualizer* also included as a side effect.

Note: there's *ShowFlag.VisualizePostProcessStack* command available, but I had lot of trouble with this.

The Lumen Switcher Actor Component automatically lists all Post Process Volumes in the level, sorted by their priority and updates its status, if camera is currently inside or not.

In addition, you can decide to add a debug draw to all PP Volumes in the level to draw the effective bounds, taking the BlendRadius settings into account.
Optionally, also visualize the relative priorities between them in color. Feel free to adjust or create your own Color Curve.

## Remarks

For sure, there's a lot more to be covered, especially about settings that are contraditcory. Not sure, if everything is checked by the engine internally for being a valid combination. Definitely needs more testing. But it turned out to be useful in my case.

* The Plugin is made for UE5.5
* The Plugin is meant to be used in Development only, see uplugin file. 
* I did not yet test, what happens, if trying to package any build of a project, if the Player Character has the Switcher Component still added.
* To build the Plugin, clone the repo and follow the standard procedure from within the Plugins window in Unreal Editor. I do not yet have zipped version.
* Yes, there's quite some C++ involved - but C++ is required for some of the functionality, and actually makes life a lot easier in many aspects, just to mention source control.
* The sample project also contains a *Size reduced* version of Manny, I called MiniManny. This consumes only 27MB compared to the >300MB from third person template - saving lot of space on Github that way.

## Some thanks 

* Thanks to XIST for providing some gitignore and gitattributes on https://github.com/XistGG/UE5-Git-Init/tree/main
* My plugin makes use of PromptFont by Yukari "Shinmera" Hafner, available at https://shinmera.com/promptfont under the SIL Open Font License.

