# Workshop description (draft, Steam BBCode)

Title: **Dark Adaptation – Realistic Night Eyes (EyeAdapt)**
Short tagline: *Night is dark again. Your eyes need time. Light has a price.*

---

```
[h1]Dark Adaptation – Realistic Night Eyes[/h1]
[i]Night is dark again. Your eyes need time. Light has a price.[/i]

[h2]The problem[/h2]
Every server picks one night brightness. Half the players find it too dark, the other half too bright, and nobody can agree. Pitch black is no fun, and bright nights take away the fear of the dark.

[h2]The idea[/h2]
Make darkness something you [b]manage[/b], not a fixed setting. Your eyes adapt like real eyes:

[list]
[*] [b]Step out of a lit base into the night:[/b] you see only the plain server night at first. Stay in the dark and your vision opens up over the next minutes: brighter image, colours fading to grey, a slight blue shift.
[*] [b]Stand at a campfire or use a flashlight:[/b] you lose your night vision. You see the fire, but not what is in the dark. Everyone out there can see you.
[*] [b]Look into bright light while fully adapted:[/b] glare and bloom blow out the view for a moment. Flares, headlights and flashlights matter.
[*] [b]Red light barely costs night vision.[/b] Red chemlights and the red headtorch finally have a real use.
[*] [b]Night vision gear is not free.[/b] Using it costs adaptation.
[*] [b]Flashbang hit:[/b] adaptation drops to zero.
[/list]

[h2]Made for hardcore survival[/h2]
[list]
[*] [b]Light discipline matters.[/b] A lit base is safe, but you will be nearly blind when you step out of it.
[*] [b]A group leaving a lit camp[/b] is at a disadvantage against a lone player who has been in the dark for a while.
[*] [b]Ambient light limits the gain.[/b] Full moon on an open field helps a lot. Overcast new moon in a forest helps little. Underground gives nothing, so bring a torch.
[*] [b]No relog trick.[/b] Adaptation is saved on the server. By default you return with the night vision you logged off with, and a server restart freezes time. Relogging gains nothing.
[*] [b]Also works on PvP servers.[/b] Dark ambushes, flares and flashbangs turn into real tactical tools.
[/list]

[h2]What it will never do[/h2]
[list]
[*] [b]Never darker than your server's baseline.[/b] Worst case is the vanilla night you configured. Adaptation only adds brightness on top.
[*] [b]Does nothing during the day.[/b]
[*] [b]Does not touch your lighting config.[/b] Your night stays as dark as you set it.
[*] [b]No blur, no aiming penalty.[/b] The mod does not simulate the blind spot in the centre of the eye.
[/list]

[h2]For server admins[/h2]
[list]
[*] Everything is configurable in [b]profiles/EyeAdapt/config.json[/b]. No repacking. Edit, restart, done.
[*] Adaptation time: fixed seconds, or a share of the measured night length, so accelerated time is covered automatically.
[*] Light table: set the strength of every light type (fire, torch, flashlight, headlights, flares, chemlights, ...).
[*] Tune brightness gain, how bright a full moon night gets, desaturation, glare strength, light falloff and more.
[*] Missing keys fall back to defaults. Your edits survive mod updates.
[*] Logout behaviour is your choice: keep the value (default), a fixed cap, or offline time counts as sleep. Saved in one small file, a server restart freezes time.
[*] Does not write into character persistence, so removing the mod cannot break characters.
[*] Full admin guide included.
[/list]

[h2]Installation[/h2]
[olist]
[*] Add the mod to the [b]server and the client[/b] mod list.
[*] Copy the .bikey to the server's keys folder.
[*] Start the server once, edit profiles/EyeAdapt/config.json, restart.
[/olist]

[h2]Honest limits[/h2]
[list]
[*] Gamma, NVIDIA filter and monitor tricks exist with or without this mod. No mod can fully stop them. Because the mod never goes below vanilla, honest players are never worse off.
[*] The effect is client-side post-processing, like any visual mod.
[/list]

[h2]Works with Terje[/h2]
Running Terje on your hardcore server? EyeAdapt sits on top of it.
[list]
[*] [b]Cat Vision (Terje Skills):[/b] the perk and the adaptation add up. A skilled player sees better, a skilled and dark-adapted player sees best. Too bright together? Lower Skills.StealthCatVisionPower in Terje's Skills.cfg or MaxEvBoost in EyeAdapt's config.json.
[*] [b]Sleeping (Terje Medicine):[/b] eyes closed means darkness. Sleep next to the campfire and you wake up dark-adapted.
[/list]
No Terje? Nothing changes. Unconscious players and players with a burlap sack over the head adapt to the dark too, in vanilla.

[h2]Compatibility[/h2]
[list]
[*] Tested with CF and VPPAdminTools loaded, on Chernarus, Livonia and Namalsk.
[*] Vanilla night vision goggles and flashbang effects take priority: while they run, EyeAdapt steps aside.
[*] Maps and mods with their own sunrise/sunset times are picked up automatically. Lights from other mods count with default values, add them to the light table (Numpad 9 with DebugKeys prints their class names).
[*] [b]Not tested yet:[/b] lighting and night overhaul mods that change exposure or glow settings, other post-processing mods. If you run one, tell me how it behaves.
[/list]

[h2]Changelog[/h2]
[b]0.4.0[/b]
[b]Server owners: update the mod on your server too.[/b] Clients get the new version from the Workshop, a server on an older version kicks them with "Client has a corrupt or modified PBO". Your config.json stays, new keys are added automatically.
[list]
[*] [b]New:[/b] eyes closed = darkness. Unconscious, burlap sack over the head, or asleep with Terje Medicine: light no longer counts, so the eyes adapt even next to a fire. No glare while the eyes are closed.
[*] [b]New:[/b] works with Terje Skills Cat Vision. The perk and the adaptation add up (before, EyeAdapt replaced the perk while adapting).
[*] [b]New:[/b] PerfLog setting for admins: logs EyeAdapt's own cost per frame. Measured in a base lit by 12 torches: about 0.07 ms per frame.
[/list]

[b]How the adaptation curve works[/b]
Two stages, like real eyes: a fast one (pupil and cones, about 20 s) and a slow one (rods, 10 minutes by default). The visual effect builds up gradually, so the first minutes show little and the last stretch counts most. Light pulls adaptation down much faster than darkness raises it.

[h2]Feedback[/h2]
Found a bug or want a feature? Open an issue on GitHub: https://github.com/olliollio/EyeAdapt/issues
Source: https://github.com/olliollio/EyeAdapt
Admin guide: https://github.com/olliollio/EyeAdapt/blob/main/docs/ADMIN.md
```

---

## Open items before publishing
- Screenshots: `screenshots/2b_vanilla.png` vs `2b_adapted.png` is the hero image. Add a glare shot and a GIF of the slow open-up.
- Fill the TODOs: compatibility, feedback link, source link.
- Check the Workshop for name clashes before final title.
- Verify claims before posting: flashbang reset (commit b902171), red headtorch, NV cost, performance statement if you add one.
- Terje: Cat Vision stacking verified in game 2026-10-28 (NA 0 vs 100 with perk stage 5). Sleep next to a burning fire verified the same day (NA rises while asleep, held low while awake).
- Decide whether to mention the Reddit post (1000+ upvotes) as social proof.
