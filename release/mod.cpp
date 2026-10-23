// Mod metadata for the DayZ launcher and the Steam Workshop upload.
// Lives in the root of the packed mod folder: @EyeAdapt\mod.cpp (next to addons\ and keys\), not inside the addon source.
// The game resolves image paths in its file system (inside the PBOs, like CF does), not in this folder: the images live
// in the PBO under EyeAdapt\data\gui (copies of release\*.paa; loose copies next to this file only show a white square).
// The Workshop preview image is set separately on the Steam page (use Artwork\logo_large.png).
name = "EyeAdapt - Eye Adaptation";
author = "ollio";
version = "0.2.0";
tooltip = "EyeAdapt";
overview = "Simulates eye adaptation at night. Stay in the dark and your vision slowly improves, light (fire, flashlight, headlights) costs night vision, and sudden bright light while dark-adapted gives a short glare. Night only, never darker than the server's own night, fully configurable by server admins. Test build.";
picture = "EyeAdapt\data\gui\picture.paa";
logo = "EyeAdapt\data\gui\logo.paa";
logoSmall = "EyeAdapt\data\gui\logo_small.paa";
logoOver = "EyeAdapt\data\gui\logo_hover.paa";
action = "";
actionURL = "";
