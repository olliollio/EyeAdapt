# EyeAdapt – Tester Guide

Thanks for helping test. This page tells you what the mod does, how to use the test keys and what to look at.

---

## 1. What the mod does

EyeAdapt simulates how eyes adapt to darkness. It only works **at night**. By day nothing changes.

- Stay in the dark for a few minutes and your vision improves: brighter image, less colour, a slight blue tint.
- Light costs night vision. Standing at a fire, using a flashlight or looking into headlights pulls you back to "light-adapted".
- If your eyes are dark-adapted and a bright light suddenly appears in front of you, you get a short glare. **Glare is switched off in this test build** (the server can enable it), so you only lose night vision from light.
- Red light barely costs night vision (red chemlight, black headtorch).
- The night is never darker than the server's normal night. The mod only adds brightness.
- Available light matters: full moon in the open gives a lot, overcast and forest give little, underground gives nothing (you need a flashlight).

Your adaptation value is called **NA** (night adaptation). 0 = light-adapted (plain server night), 1 = fully dark-adapted (full effect).

---

## 2. On screen

If the server has it on, a small text in the top left shows your NA, for example `NA 73 %`. It updates once per second.

---

## 3. Test keys

Use the Numpad, with NumLock on. The keys are ignored while a menu or the chat is open. Each key shows a short message on screen. They only affect **your own game**.

| Key | What it does |
|---|---|
| Numpad 1 | Set NA to 0 (as if you just left a lit room) |
| Numpad 3 | Set NA to 1 (fully dark-adapted) |
| Numpad 7 | Effect on/off, to compare with the normal vanilla night |
| Numpad 9 | Light scan now: shows total dose and the strongest light on screen, lists every light in the script log |
| Numpad * | Force full glare on/off, to judge how the glare looks |
| Numpad / | Toggle test headlights on the nearest car within 30 m. Lets you test oncoming headlights alone. The real lights go off when you get out |

If you try to use the keys and nothing happens, the server has the debug keys off, or the Numpad key is bound to something else in another mod.

---

## 4. What to test

Test at night. Fullscreen only: windowed mode lifts the blacks and makes the night look brighter than it is.

### 4.1 Basic adaptation

1. Press Numpad 1 (NA 0), then stand still in the dark away from lights.
2. Watch the picture over a few minutes: it should get brighter, colours fade, slight blue tint. Compare with Numpad 7 (effect off).
3. Is the improvement noticeable but not too much? Does it look natural, or too grey/foggy?

### 4.2 Light costs night vision

1. Press Numpad 3 (NA 1). Walk to a campfire, torch or lit lamp and stand next to it for a minute.
2. NA should fall fast. Walk back into the dark: your view should be dark again and then slowly improve.
3. Try other lights: flashlight, headtorch, chemlights (white, green, red), road flare, flare gun, car headlights.
4. Red light (red chemlight, black headtorch) should cost far less than white.

### 4.3 Glare (skip unless the server has it on)

The test build ships with glare off. Only test this section if the server owner set `GlareStrength` above 0.

1. Press Numpad 3. Face a bright light from a short distance: a flashlight pointed at you, a lit lamp, headlights.
2. You should get a short white-out, then a glow that fades over a few seconds.
3. Facing away or looking to the side should do nothing.
4. Does the glare look good, is it too long or too strong? Press Numpad * to see it on demand.
5. Repeating glare in a short time should get weaker (30 s recovery).

### 4.4 Other players' lights (needs a second person)

1. Both dark-adapted. One player points a flashlight at the other from a few metres.
2. If the server has flashlight dazzle on, the other player gets a short white-out, once per beam contact.
3. Does it feel fair? Can you still see the other player in the dark?

### 4.5 Cars

- Sit in a car with the headlights on. You should lose some night vision (you look at the lit road).
- Stand in front of a car with the headlights on and look at them.
- Drive past an oncoming car.

### 4.6 Night vision

- Put on powered NV goggles (or aim through a powered NV scope). NA should fall while they are on.
- After taking them off you should be partly night-blind and recover slowly.
- While NV is on there is no glare and no extra night look.

### 4.7 Flash grenade

- A flash grenade hit should send NA back to 0, on top of the vanilla white-out.

### 4.8 Weather and place

- Compare clear and overcast nights, and moon phases if possible.
- Open field vs forest vs underground (Livonia bunker, Sakhal): underground should give no brightness gain, only a flashlight helps.

### 4.9 Logout and login

- Go to NA 1, log out and back in right away: you should keep roughly the same NA.
- Stay logged out for a while: with the default `LogoutMode` "keep" NA should be the same as when you left.
- Restart the server while logged in: NA should be the same after the restart.
- A quick relog should not give you more than waiting in the dark would have.

### 4.10 Day and dusk

- At dusk, if you stay outside, nothing odd should happen: the effect fades in slowly after sunset.
- At dawn it should fade out before sunrise. Check that it does not look washed out.
- By day the mod should do nothing.

---

## 5. How to report

For each finding, send:

- What you did and what you saw, in a few words.
- Place, time of day, weather, and the moon if you know it.
- The NA % shown on screen.
- What you expected instead.

Useful extras:

- A **screenshot or short clip** (with and without Numpad 7).
- For light problems: press **Numpad 9** next to the light and send the lines from your script log. They name the light and show how it was counted. Logs are in `%LocalAppData%\DayZ\`, the newest `script_*.log`.
- Your graphics settings and the brightness/gamma setting if the night looked too dark or too bright.

Things we especially want to know:

1. Is the night playable without gamma tricks?
2. Does adaptation take too long or too short?
3. Does light punish too much or too little?
4. Is the glare fun or annoying?
5. Anything that looks broken (flicker, stuck white-out, effect not switching off by day).

---

## 6. Please note

- Numpad 1 and 3 change your NA instantly. Do not use them while testing timing, only to set a starting point.
- Test results with the debug keys are not representative of normal play.
- The numbers (adaptation time, glare strength, how much light costs) are first guesses. Honest feedback on how it feels is the most useful thing you can send.
