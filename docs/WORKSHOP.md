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
[*] [b]No relog trick.[/b] Adaptation is saved on the server, and offline time counts only as time in the dark. Relogging gains nothing beyond waiting.
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
[*] Tune brightness gain, desaturation, glare strength, light falloff and more.
[*] Missing keys fall back to defaults. Your edits survive mod updates.
[*] Per-player data in separate JSON files with hashed IDs. Delete a file to reset a player.
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

[h2]Compatibility[/h2]
[TODO: test with common lighting / night mods and fill in]

[h2]Feedback[/h2]
Found a bug or want a feature? [TODO: link to GitHub issues or Discord]
Source: [TODO: github.com/olliollio/EyeAdapt, if you want it public]
```

---

## Open items before publishing
- Screenshots: `screenshots/2b_vanilla.png` vs `2b_adapted.png` is the hero image. Add a glare shot and a GIF of the slow open-up.
- Fill the TODOs: compatibility, feedback link, source link.
- Check the Workshop for name clashes before final title.
- Verify claims before posting: flashbang reset (commit b902171), red headtorch, NV cost, performance statement if you add one.
- Decide whether to mention the Reddit post (1000+ upvotes) as social proof.
