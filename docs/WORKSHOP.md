# Workshop description (live text, Steam BBCode)

Title: **EyeAdapt – Eye Adaptation**
Paste the block below into the Workshop description. The older long draft is in the git history of this file.

---

```
[h1]EyeAdapt – Eye Adaptation[/h1]
[i]Night is dark again. Your eyes need time. Light has a price.[/i]

[h2]What it does[/h2]
[list]
[*] [b]Stay in the dark[/b] and your vision slowly improves: brighter image, less colour, a slight blue tint.
[*] [b]Light costs night vision:[/b] campfires, flashlights, headlights, flares. [b]Red light[/b] barely costs any.
[*] [b]Two stages like real eyes:[/b] a fast first improvement (about 20 s) and a slow rod stage (10 min by default). The first minutes show little, the last stretch counts most.
[*] [b]Night only.[/b] By day nothing changes. Never darker than the server's own night.
[*] [b]Full moon nights are capped[/b] (FullMoonShare), so dark real-time servers don't get too bright.
[*] [b]Eyes closed = darkness.[/b] Unconscious or with a burlap sack over your head, light doesn't count: your eyes adapt even next to a fire. No glare while your eyes are closed.
[*] [b]Flash grenades[/b] reset your adaptation.
[*] [b]Flashlight dazzle:[/b] someone else's beam in your face blinds you briefly (on by default, server option).
[*] [b]No relog trick.[/b] Adaptation is saved on the server and survives restarts. By default you log back in with the night vision you logged off with, and a server restart freezes time. Admins can choose a fixed cap or "offline counts as sleep" instead (LogoutMode).
[/list]

[h2]Works with Terje[/h2]
Optional. Without Terje nothing changes.
[list]
[*] [b]Cat Vision (Terje Skills):[/b] the perk and the adaptation add up. A skilled player sees better, a skilled and dark-adapted player sees best. Light still costs both. Too bright together? Lower [b]MaxEvBoost[/b] in config.json.
[*] [b]Sleeping (Terje Medicine):[/b] counts as eyes closed. Sleep next to the campfire and you wake up dark-adapted.
[/list]

[h2]Server owners[/h2]
[list]
[*] Load the mod on the [b]server AND the clients[/b].
[*] Copy [b]keys\EyeAdapt.bikey[/b] into the server's keys folder.
[*] Start the server once: it creates [b]profiles\EyeAdapt\config.json[/b]. New keys are added on updates, your values stay.
[*] Fully configurable, including a light table per light type.
[*] Player adaptation is saved in [b]profiles\EyeAdapt\adaptation.json[/b] (one small file).
[*] Lightweight: about 0.07 ms per frame in a base lit by 12 torches. The PerfLog setting measures it on your server.
[*] Debug keys (Numpad) are off by default.
[/list]

[h2]Docs[/h2]
[list]
[*] [url=https://github.com/olliollio/EyeAdapt/blob/main/docs/ADMIN.md]Admin guide[/url] – every setting, example presets
[*] [url=https://github.com/olliollio/EyeAdapt/blob/main/CHANGELOG.md]Changelog[/url]
[*] [url=https://github.com/olliollio/EyeAdapt]Source and issues[/url]
[/list]
```

---

## Change note for the Publisher upload (0.4.0, Steam BBCode)

```
[h2]0.4.0[/h2]
[b]Server owners: update the mod on your server too.[/b] Your config.json stays, new keys are added automatically.

[h3]New[/h3]
[list]
[*] [b]Eyes closed = darkness.[/b] Unconscious, burlap sack over the head, or asleep with Terje Medicine: light no longer counts, so the eyes adapt even next to a fire. No glare while the eyes are closed.
[*] [b]Terje Skills Cat Vision:[/b] the perk and the adaptation now add up. Before, EyeAdapt replaced the perk's boost while adapting.
[*] [b]PerfLog[/b] setting for admins: logs EyeAdapt's own cost. Measured in a base lit by 12 torches: about 0.07 ms per frame.
[*] [b]New logo:[/b] an owl with big orange eyes.
[/list]

[h3]Fixed[/h3]
[list]
[*] The PerfLog line was printed as [i]string line = '...'[/i].
[/list]
```

---

## Notes
- Terje: Cat Vision stacking verified in game 2026-10-28 (NA 0 vs 100 with perk stage 5). Sleep next to a burning fire verified the same day (NA rises while asleep, held low while awake). Burlap sack and knock-out verified the same day.
- Screenshot ideas: vanilla vs adapted as the hero image, a glare shot, a GIF of the slow open-up.
