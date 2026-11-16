# Menu UI spec: front end, pause, settings and stats

Status: **draft v0.1, for review**. Owner: Agent 7 (UI). Implements GDD sections 7, 10 (UI), 11 mode 1 and 12 item 1.
Reads with: [`docs/GDD.md`](../GDD.md), [`docs/decisions/0003-ui-and-presentation.md`](../decisions/0003-ui-and-presentation.md).

This document describes every menu screen in the game, every element on each screen, its states, how a gamepad moves between them, what each one says, and the design tokens they're built from. If this spec and the GDD disagree, the GDD wins. If this spec and the code disagree, this spec is the target and section 2 lists the gaps.

The in-match HUD (score bug, callouts, timing cue, score card, golden point prompt) is **not** redesigned here. It only picks up the shared tokens from section 4 so menus and HUD feel like one product (section 7.11).

---

## Contents

1. [Principles](#1-principles)
2. [What exists today, and the gaps](#2-what-exists-today-and-the-gaps)
3. [Screen flow](#3-screen-flow)
4. [Design tokens](#4-design-tokens)
5. [Input and focus model](#5-input-and-focus-model)
6. [Components](#6-components)
7. [Screens](#7-screens)
8. [Copy deck](#8-copy-deck)
9. [Motion](#9-motion)
10. [Accessibility](#10-accessibility)
11. [Implementation notes](#11-implementation-notes)
12. [Slice scope: must, if time, post-slice](#12-slice-scope-must-if-time-post-slice)
13. [Open decisions for Caio](#13-open-decisions-for-caio)

[Appendix A. Prompt for Claude Design](#appendix-a-prompt-for-claude-design)

---

## 1. Principles

1. **Gamepad first, few presses to play.** From launch to the first serve with the defaults takes 3 presses: any button (Title), A (Quick Match tile), A (Start match, which is the setup screen's default focus). The Versus card plays itself out, or A skips it. Every screen works with a gamepad alone (GDD 12 item 1), and keyboard and mouse are full fallbacks.
2. **Always know what A and B do.** Every screen has a focused element at all times and a button hint bar that names what each live button does right now.
3. **Premium, but simple.** The look of a TV sports broadcast: dark glass panels over a live court, one accent colour, big condensed type. Fewer screens and fewer options than a full sports sim; nothing that the slice can't actually do is clickable.
4. **The court is the background.** Menus sit over a slow camera move on the real court level, not over a static image. The glass panels are a nod to the court's glass walls.
5. **Readable from the couch.** Built at 1920×1080 and read from 2.5 m on a TV. No text is smaller than 26 px, and the focus state never relies on colour alone.
6. **Original.** Layouts, iconography and copy are our own. No borrowed layouts from any existing sports game (GDD 1 originality rule), no real brands, players, tours or logos. "Similar to a modern football sim" means the same level of polish, not the same screens.
7. **Logic in C++, values in data.** Every layout is built in C++ in `BuildTree()` (decision 0003). Colours, sizes and motion timings are tokens read from `Data/UiTheme.json` (section 11.2), not numbers scattered through widgets.

---

## 2. What exists today, and the gaps

Built by Agent 7 in PRs 10 and 15 (`Source/Padel/{Public,Private}/UI/`):

| Today | File | Notes |
|---|---|---|
| Screen flow `MainMenu → MatchSetup → Match → Paused → Stats` | `APadelHUD`, `EPadelScreen` | Works end to end with a gamepad, and plays `FDemoMatch` if no simulation is attached. |
| One generic menu layout for every screen | `UPadelMenuWidget` | Centred card: title, body, optional table, a vertical column of buttons. Left/right cycles a setup option; B, Esc or Start goes back. |
| Base widget with C++-built trees | `UPadelWidgetBase` | `MakeText`, `MakePanel`, `MakeMenuButton`, focus helpers. |
| Colour and type constants | `PadelUiStyle.h` | Team colours teal `#2EC4B6` (Lagoon) and orange `#FF7A3D` (Ember), accent yellow `#FFD23F`, four font sizes, 48 px safe margin, engine default font. |
| Setup options | `Padel::Ui::FMatchSetup` | Difficulty (Easy, Medium, Hard), 1 set or best of 3, golden point on/off, which Team A player the human controls. |
| Stats | `Padel::Ui::FMatchStats` | Points won, winners, errors, double faults, shots off the glass, points-ended-by reasons, average rally, share of points with glass. |
| Roster | `Padel::Ui` roster | Selva Arunde and Mikko Ventara (Team Lagoon), Ilan Corvisa and Ruben Halvaro (Team Ember). |
| Pad glyph names | `Padel::Ui::GamepadKeyLabel` | Text labels per pad family (Xbox, PlayStation, Switch), no icons yet. |

**Gaps this spec closes** (each is a work item in section 11.4):

| # | Gap | Where in this spec |
|---|---|---|
| G1 | No title screen; the game opens straight on the main menu. | 7.1 |
| G2 | The main menu is two buttons (Quick Match, Quit). No tiles, no Practice entry, no Settings. | 7.2 |
| G3 | Practice (`APadelPracticeDirector`, GDD 12 item 3) can't be reached from the menu. | 7.2, 7.9 |
| G4 | Setup is four cycling text rows. No player cards, no ratings, no view of the opponents. | 7.3 |
| G5 | No overall rating is computed anywhere; the GDD 6.2 weights aren't defined. | 6.6, 13 Q1 |
| G6 | No Controls screen, though GDD 7 says every action must be shown with the right glyphs. | 7.6 |
| G7 | No Settings screen (audio, display, gameplay, accessibility). | 7.7 |
| G8 | No button hint bar on any screen. | 6.1 |
| G9 | Pause has Resume, Restart, Quit, but no Controls or Settings, and Restart and Quit act with no confirmation. | 7.5, 7.10 |
| G10 | Quit (to desktop) acts immediately, with no confirmation. | 7.10 |
| G11 | The stats screen is a plain text table with no score by set and no smash count. | 7.8 |
| G12 | Tokens are hard-coded constants, not data; the font is the engine default. | 4, 11.2 |
| G13 | `Start` (Gamepad_Special_Right) is treated as Back on every menu screen. It should only resume from Pause. | 5.2 |
| G14 | Focus always resets to the first button when a screen opens; it should return to where you were. | 5.4 |
| G15 | No mouse hover state; the mouse cursor shows but the focus visuals only follow keyboard focus. | 5.6 |

---

## 3. Screen flow

```
                 ┌────────────┐
  boot ────────► │ 7.1 Title  │
                 └─────┬──────┘
                       │ any button
                       ▼
                 ┌────────────┐   B / Esc   ┌──────────────────┐
         ┌─────► │ 7.2 Main   │ ──────────► │ 7.10 Quit game?  │ ──A──► desktop
         │       │    menu    │ ◄────B───── └──────────────────┘
         │       └┬────┬────┬─┘
         │  Quick │    │    │ Settings
         │  Match │    │    └──────────────► 7.7 Settings ──B──► back to caller
         │        │    │ Practice
         │        │    └──────────────────► 7.9 Practice (court, ball machine)
         │        ▼                              │ Start ► Practice pause ► Quit ► Main menu
         │  ┌────────────┐
         │  │ 7.3 Match  │ ──B──► Main menu
         │  │    setup   │
         │  └─────┬──────┘
         │        │ Start match
         │        ▼
         │  ┌────────────┐
         │  │ 7.4 Versus │ (2.5 s, skippable with A)
         │  └─────┬──────┘
         │        ▼
         │  ┌────────────┐  Start  ┌───────────┐ ── Controls ─► 7.6 Controls ──B──┐
         │  │   Match    │ ──────► │ 7.5 Pause │ ── Settings ─► 7.7 Settings ──B──┤
         │  │   (HUD)    │ ◄─────  │           │ ◄────────────────────────────────┘
         │  └─────┬──────┘ Resume  └──┬─────┬──┘
         │        │ match won         │     │ Restart ► 7.10 confirm ► 7.4 Versus
         │        ▼ (4 s card)        │     │
         │  ┌────────────┐            │     └ Quit to menu ► 7.10 confirm ─┐
         │  │ 7.8 Stats  │ ◄──────────┘ (no)                               │
         │  └┬──────────┬┘                                                 │
         │   │ Rematch  │ Main menu                                        │
         │   ▼          └──────────────────────────────────────────────────┤
         │  7.4 Versus                                                     │
         └─────────────────────────────────────────────────────────────────┘
```

**`EPadelScreen` grows to:** `Title, MainMenu, MatchSetup, Versus, Match, Paused, Controls, Settings, Stats, Practice, PracticePaused`. Confirm dialogs (7.10) are modal layers, not screens: the screen under them stays the same.

**Back (B, Esc) on every screen:**

| Screen | B does |
|---|---|
| Title | Nothing (there's nowhere to go back to). |
| Main menu | Opens "Quit game?" (7.10). |
| Match setup | Main menu. Setup choices are kept for the session. |
| Versus | Nothing. A skips it. |
| Pause | Resume (same as Start). |
| Controls, Settings | Back to whichever screen opened them (main menu or Pause). Settings changes are already applied (7.7). |
| Stats | Nothing, so a stray press can't throw away the stats. Leave with the Main menu or Rematch button. |
| Any confirm dialog | Closes the dialog, focus returns to the button that opened it. |

**Screen-to-screen rule:** at most one transition animation plays at a time; input is ignored during the 120 ms entry of a screen (section 9), so a double press can't skip past a screen by accident.

---

## 4. Design tokens

All tokens live in `Data/UiTheme.json` (section 11.2) and are read into a `FPadelUiTheme` struct that replaces the constants in `PadelUiStyle.h`. The names below are the JSON keys.

### 4.1 Colour

The palette is deep navy and charcoal with one accent, electric lime. Lime is used **only** for focus, the primary call to action and "selected" marks. It is never used for text blocks or large fills, and never in the in-match HUD, where the ball must be the brightest thing on screen (GDD 10).

| Token | Hex | Alpha | Use |
|---|---|---|---|
| `Colour.Bg.Deep` | `#070B14` | 1.0 | Behind everything when there's no court (boot, loading). |
| `Colour.Bg.Scrim` | `#070B14` | 0.55 | Full-screen dim over the court behind menus. Pause uses 0.70. |
| `Colour.Bg.Vignette` | `#000000` | 0 → 0.6 | Radial, edges darker, so panels on the left and right read. |
| `Colour.Glass.Fill` | `#111A2B` | 0.72 | Glass panel fill (tiles, cards, lists, dialogs). |
| `Colour.Glass.FillRaised` | `#1A2540` | 0.82 | A focused panel's fill. |
| `Colour.Glass.Edge` | `#FFFFFF` | 0.10 | 1 px inner stroke on every glass panel (the glass edge highlight). |
| `Colour.Glass.EdgeTop` | `#FFFFFF` | 0.22 | 1 px top edge only, a light catch on the glass. |
| `Colour.Surface.Charcoal` | `#151922` | 0.94 | Solid panels where glass would hurt readability: the hint bar, dialogs, dropdown lists. |
| `Colour.Accent` | `#C8F03C` | 1.0 | Electric lime. Focus ring, primary button fill, selected marks, toggles on. |
| `Colour.Accent.Pressed` | `#A6CC22` | 1.0 | The accent while A is held. |
| `Colour.Accent.Glow` | `#C8F03C` | 0.35 | Outer glow on the focus ring, 12 px blur. |
| `Colour.OnAccent` | `#0A1020` | 1.0 | Text and icons on a lime fill. |
| `Colour.Text.Primary` | `#F4F1E8` | 1.0 | Main text. Same warm white as today's `PadelUiStyle::Text`. |
| `Colour.Text.Secondary` | `#F4F1E8` | 0.68 | Descriptions, labels, the stat names. |
| `Colour.Text.Disabled` | `#F4F1E8` | 0.34 | Disabled and locked labels. |
| `Colour.Team.Lagoon` | `#2EC4B6` | 1.0 | Team A, unchanged from today. |
| `Colour.Team.Ember` | `#FF7A3D` | 1.0 | Team B, unchanged from today. |
| `Colour.State.Good` | `#7CFF6B` | 1.0 | HUD timing "Perfect" only (unchanged). |
| `Colour.State.Warning` | `#FFB347` | 1.0 | Warnings in Settings ("Restart needed"). |
| `Colour.State.Danger` | `#FF5D5D` | 1.0 | Destructive confirm buttons (Quit game). |
| `Colour.Lock` | `#8C95A8` | 1.0 | Padlock icon and "Coming soon" badge text. |

**Contrast** (against `Colour.Glass.Fill` composited over the scrimmed court, measured on the darkest and brightest court frames): `Text.Primary` ≥ 12:1, `Text.Secondary` ≥ 6:1, `Text.Disabled` ≥ 3:1 (allowed for disabled only), `OnAccent` on `Accent` 13.8:1. Team colours are used for labels at 28 px bold and up, where they pass 4.5:1.

**Why lime and not today's yellow:** the yellow `#FFD23F` is too close to the ball's yellow. Lime reads as "padel ball" in the menus without being confused with the ball in play. The HUD keeps its current yellow accent for callouts until the HUD pass decides (13 Q4).

### 4.2 Type

One family, **Barlow** (SIL Open Font Licence 1.1), in two widths: **Barlow Condensed** for display and labels, **Barlow** for body. Both ship as `.ttf` under `SourceArt/Fonts/` with a row each in `SourceArt/manifest.csv` (source URL and OFL), and import as `UFontFace`/`UFont` assets in `Content/Padel/UI/Fonts/`. Numbers use tabular figures (`tnum`) everywhere scores or stats line up.

| Token | Family, weight | Size / line height (px at 1080p) | Tracking | Case | Use |
|---|---|---|---|---|---|
| `Type.Hero` | Condensed ExtraBold Italic | 120 / 108 | −1% | UPPER | Title logo text, winner name on Stats. |
| `Type.Display` | Condensed Bold | 72 / 72 | 0 | UPPER | Screen titles ("QUICK MATCH"). |
| `Type.Score` | Condensed Bold | 96 / 96 | 0 | — | The set scores on Stats and Pause. Tabular. |
| `Type.Title` | Condensed SemiBold | 44 / 48 | +1% | UPPER | Large tile label, card names, section headers. |
| `Type.Heading` | Condensed SemiBold | 32 / 36 | +2% | UPPER | Small tile labels, list button labels, tab labels. |
| `Type.Body` | Barlow Medium | 28 / 36 | 0 | Sentence | Descriptions, settings labels. |
| `Type.BodySmall` | Barlow Medium | 26 / 34 | 0 | Sentence | Hint bar labels, helper text. Smallest allowed size (today's `FontSmall` is 22, too small). |
| `Type.Label` | Condensed Bold | 26 / 30 | +8% | UPPER | Badges, breadcrumbs, small captions. |
| `Type.Stat` | Condensed Bold | 40 / 40 | 0 | — | Stat numbers, overall rating on small cards. Tabular. |
| `Type.Overall` | Condensed ExtraBold | 88 / 80 | −2% | — | Overall rating on the large player card. Tabular. |

Text sizes scale with the DPI rule in 4.4. The **Text size** setting (7.7) multiplies Body, BodySmall and Label by 1.0 (Default), 1.15 (Large) or 1.3 (Largest), and layouts must hold at Largest (section 10).

### 4.3 Spacing, grid, shape

- **Base unit 8 px.** Tokens `Space.1` to `Space.8` = 8, 16, 24, 32, 48, 64, 96, 128.
- **Safe area:** `Safe.X` 96 px left and right, `Safe.Y` 64 px top and bottom. That's inside the 90% title-safe area (96 × 54 px at 1080p), with 10 px to spare top and bottom for TVs that overscan. Nothing interactive or textual goes outside it, only backgrounds. This replaces the 48 px `SafeMargin`.
- **Grid:** 12 columns inside the safe area (1728 px wide): column 122 px, gutter 24 px (12 × 122 + 11 × 24 = 1728). A span of n columns is 146n − 24 px wide: 3 = 414, 4 = 560, 5 = 706, 6 = 852, 8 = 1144. Screens below give positions as columns.
- **Header band:** top of safe area, 96 px tall. Holds the screen title (left) and a context line (right).
- **Hint bar band:** bottom of safe area, 56 px tall (6.1).
- **Content area:** y 184–936, between the header and hint bar bands with a 24 px gap from each: 1080 − 64 − 96 − 24 − 24 − 56 − 64 = **752 px** tall.
- **Radius:** `Radius.Panel` 12 px (tiles, cards, dialogs), `Radius.Control` 8 px (buttons, selectors), `Radius.Pill` 999 px (badges, toggles, chips).
- **Glass panel recipe:** `Glass.Fill` + 1 px `Glass.Edge` + 1 px `Glass.EdgeTop` + drop shadow `#000000` at 0.45, 0 × 12 px offset, 32 px blur. Panels are **plain translucent fills, with no blur of their own**.
- **One blur per screen, at most.** The frosted look comes from a single full-screen `BackgroundBlur` layer (strength 10) placed under all the panels and over the backdrop, or from depth of field on the menu camera, which is cheaper still. Each UMG `BackgroundBlur` is a full-resolution pass over a live Lumen scene, so per-panel blur would break GDD 1's 60 fps budget on an M2 Pro (Metal), and menus count. Pause reuses the same single layer. No blur at all when **Reduce transparency** is on or the GPU tier is Low.
- **Focus ring:** 3 px `Colour.Accent` outside the element's edge with 2 px gap, matching its radius, plus `Accent.Glow`. Focus also scales the element to 1.03 (tiles and cards) or 1.0 (list rows, which slide 8 px right instead) so focus is shown by shape and motion, not colour alone.

### 4.4 Resolution and scaling

- Authored at **1920×1080**. UMG DPI scaling uses the shortest side, 1080 = 1.0 (720p = 0.667, 1440p = 1.333, 2160p = 2.0), set in `DefaultEngine.ini` `[/Script/Engine.UserInterfaceSettings]`.
- Wider than 16:9 (21:9, 32:9): the layout stays 16:9 and centred, and backgrounds and the scrim extend to the edges. Narrower (16:10, Mac laptops): the safe area keeps its 96 px sides and the content area grows taller; tiles keep their aspect ratio and the extra height goes to the gaps between rows. Must be checked on a 16:10 Mac display (CLAUDE.md platforms).

### 4.5 Icons and glyphs

- **Button glyphs:** a circle 40 px (hint bar) or 32 px (inline) with the button's letter or symbol, per pad family from `Padel::Ui::EPadFamily`. Xbox letters A/B/X/Y; PlayStation shapes ✕ ○ □ △ drawn as our own vector shapes; Switch letters A/B/X/Y in their physical positions (A is right, B is bottom). Shoulder and trigger glyphs are rounded rectangles labelled LB/RB/LT/RT, L1/R1/L2/R2 or L/R/ZL/ZR. Keyboard glyphs are a key cap with the key's name.
- Glyphs are **our own drawings**, built procedurally (simple shapes and the Barlow font), not copied from any platform's art. The face-button colour coding that pads use is not reproduced; all glyphs are `Text.Primary` on `Surface.Charcoal` with a 2 px `Glass.Edge` ring.
- **Mapping is by position, not label** (GDD 7, `GamepadKeyLabel`): "confirm" is `Gamepad_FaceButton_Bottom` on every pad, so on a Switch pad the confirm glyph shows "B". This is a known Switch-layout trade-off (13 Q3).
- **UI icons** (padlock, check, gear, controller, ball, court): 2 px stroke line icons on a 32 px grid, built as SVG in `SourceArt/UI/Icons/` by the asset pipeline. Original drawings.

---

## 5. Input and focus model

### 5.1 Buttons in menus

| Input | Gamepad (Xbox label) | Keyboard | Mouse | Effect |
|---|---|---|---|---|
| Navigate | Left stick, D-pad | Arrow keys, WASD | Move | Moves focus to the next element in that direction (5.3). |
| Confirm | A | Enter, Space, J | Left click | Activates the focused element. |
| Back | B | Esc, Backspace, K | Right click, Mouse back | Section 3 table. |
| Change value | D-pad / stick left and right on a selector | Left/Right, A/D | Click the arrows | Cycles a selector (6.8), moves a slider (6.10). |
| Tab left / right | LB / RB | Q / E | Click a tab | Switches tabs (Settings, Controls, Stats). |
| Secondary action | X | R | — | Per screen: "Reset to default" in Settings, "Swap player" in Setup. Shown in the hint bar only when live. |
| Tertiary action | Y | F | — | Unused in the slice (post-slice: Details on a player card, 6.6). |
| Pause / resume | Start (Menu) | Esc in match | — | Opens Pause in a match; on the Pause screen, resumes. Does nothing on other menu screens. |
| Skip | A | Enter | Click | Skips the title fade and the Versus card. |

`Gamepad_Special_Left` (View / Share / −) does nothing in menus for the slice.

### 5.2 Fixes to today's handling

- Today `UPadelMenuWidget::NativeOnKeyDown` treats `Gamepad_Special_Right` (Start) as Back everywhere. Change: Start is only "Resume" on Pause and "Pause" in Match (G13).
- Today A/D cycle setup options and also move focus is done by Slate. Keep both, but WASD must also navigate (Slate's default navigation config only uses arrows and the stick); add a custom `FNavigationConfig` that maps W/A/S/D and the D-pad, with the left stick dead zone 0.5.

### 5.3 Navigation rules

- **Explicit focus maps, not geometry guessing.** Each screen in section 7 has a table of up/down/left/right targets for every focusable element. Implement them with `UWidget::SetNavigationRuleExplicit`, so focus never jumps somewhere surprising when the layout changes.
- **Wrap:** vertical lists wrap top↔bottom. Grids (main menu tiles) and horizontal rows don't wrap; at an edge the input does nothing and plays the "edge" sound (no visual shake).
- **Repeat:** holding a direction repeats after 400 ms, then every 110 ms. Holding left/right on a slider repeats every 60 ms and steps by 1, then by 5 after 1 s.
- **Locked and disabled elements:** locked tiles (6.3) **are** focusable, so the player can read why they're locked; A on them shows a toast (6.13) and does nothing else. Disabled controls (6.2) are **skipped** by navigation.

### 5.4 Focus memory

- When a screen opens fresh, focus goes to its **default focus** (named per screen in section 7).
- When you come **back** to a screen (B from a child screen, closing a dialog), focus returns to the element that opened the child.
- Match setup always opens on **Start match** (principle 1); the cards and rules are one press away (Left, or Up).

### 5.5 Device switching and glyphs

- The hint bar and all inline glyphs follow the **most recently used device** (`UInputDeviceSubsystem`, as `APadelHUD::ButtonLabel` does today), switching within one frame of input from a new device.
- Switching from mouse to pad hides the cursor and puts focus on the element last hovered (or the default). Switching from pad to mouse shows the cursor; focus stays where it was until the mouse hovers something.

### 5.6 Mouse

- Hover moves focus (hover = focus: one visual state for both, G15). Clicking a selector's arrows steps it; clicking its centre steps forward. Mouse wheel scrolls lists and steps a focused slider.
- The cursor hides after 3 s without mouse movement.

---

## 6. Components

Each component lists its anatomy, sizes at 1080p, states and behaviour. All are C++ `UPadelWidgetBase` subclasses (section 11.1). State visuals are in a single table per component; "focused" always adds the focus ring of 4.3 unless it says otherwise.

**State vocabulary used everywhere:**

| State | Meaning |
|---|---|
| Default | Visible, interactive, not focused. |
| Focused | Has gamepad/keyboard focus or mouse hover. |
| Pressed | A (or click) is held down on it. Lasts while held, minimum 80 ms. |
| Selected | The current value in a group (a tab, a chosen segment, the player you control). Can combine with focused. |
| Disabled | Not available right now (for example, Apply in Settings with no changes). Skipped by navigation. |
| Locked | Not in this build (Career, Team Builder). Focusable, explains itself, can't be entered. |

### 6.1 Button hint bar (`UPadelHintBar`)

The strip at the bottom of every menu screen that says what each live button does.

- **Anatomy:** a row of hint items, right-aligned, inside the hint bar band (4.3). Each item: glyph (40 px) + 8 px gap + label (`Type.BodySmall`, `Text.Primary`). 32 px between items. Optional left-aligned context text (`Type.BodySmall`, `Text.Secondary`), for example the Settings description line.
- **Background:** none on screens with a scrim; on Pause it sits on a full-width `Surface.Charcoal` strip at 0.8 alpha, 88 px tall, so it reads over the frozen match.
- **Order, left to right, everywhere:** A first, then X, Y, then LB RB as a pair ("LB RB  Change tab"), then B last. The group is right-aligned, so B always sits at the right edge and A always leads. Every hint list and mock-up in this spec uses this order.
- **States:** an item exists only while its action is live. A hint whose action becomes unavailable (for example, X "Reset" when everything is already default) is removed, not greyed. When A is pressed anywhere, the A glyph flashes `Accent` for 120 ms.
- **Behaviour:** items are clickable with the mouse and do the same as the button.
- **Copy:** one or two words, a verb first: "Select", "Back", "Change tab", "Swap player", "Reset", "Resume", "Skip".

### 6.2 Buttons

Three kinds.

**Primary button (`UPadelPrimaryButton`)** — the one main action on a screen ("Start match", "Rematch", "Quit game" in a dialog). At most one per screen.

- Size: 448 × 80 px (Start match), or 320 × 72 px in dialogs. `Radius.Control`.
- Label: `Type.Title`, centred, upper case, with an optional leading glyph (the A glyph) at 32 px.

| State | Fill | Label | Other |
|---|---|---|---|
| Default | `Accent` at 0.18 alpha over `Glass.Fill` | `Text.Primary` | 2 px `Accent` stroke |
| Focused | `Accent` | `OnAccent` | Focus glow, scale 1.03 |
| Pressed | `Accent.Pressed` | `OnAccent` | Scale 0.98, 80 ms |
| Disabled | `Glass.Fill` | `Text.Disabled` | No stroke; skipped by navigation |
| Danger variant, focused | `State.Danger` | `#FFFFFF` | Used only for "Quit game" |

**Secondary button (`UPadelSecondaryButton`)**: the alternative next to a primary button ("Main menu" on Stats, the safe option in dialogs).

- Size: 240 × 64 px (Stats) or 320 × 72 px (dialogs). `Radius.Control`. Label `Type.Heading`, centred, upper case.

| State | Fill | Label | Other |
|---|---|---|---|
| Default | `Glass.Fill` | `Text.Secondary` | 2 px `Glass.Edge` stroke |
| Focused | `Glass.FillRaised` | `Text.Primary` | Focus ring + glow, scale 1.03 |
| Pressed | `Glass.FillRaised` | `Text.Primary` | Scale 0.98, 80 ms |
| Disabled | `Glass.Fill` at 0.5 | `Text.Disabled` | No stroke; skipped by navigation |

**List button (`UPadelListButton`)** — vertical menus (Pause, dialogs' secondary options, Controls and Settings categories).

- Size: full width of its column × 72 px. Label `Type.Heading`, left-aligned with 32 px padding. Optional right-side value or chevron `›`.

| State | Fill | Label | Other |
|---|---|---|---|
| Default | transparent | `Text.Secondary` | — |
| Focused | `Glass.FillRaised` | `Text.Primary` | 6 px `Accent` bar on the left edge, label slides 8 px right (not the ring; rows are too close for it) |
| Pressed | `Glass.FillRaised` | `Text.Primary` | Bar widens to 10 px, 80 ms |
| Disabled | transparent | `Text.Disabled` | Skipped |

### 6.3 Tile (`UPadelMenuTile`)

The main menu's large picture buttons.

- **Sizes:** **Hero** 8 columns × 520 px (1144 × 520); **Small** 4 columns × 248 px (560 × 248), or 208 px tall in the main menu's bottom row. `Radius.Panel`, clips its image.
- **Anatomy, bottom to top:** full-bleed image (a render of the court or players from the asset pipeline; 7.2 lists which), a bottom gradient `#070B14` 0 → 0.85 over the lower 55%, an optional badge top-left (6.4), the label bottom-left (`Type.Title` on Hero at 56 px, `Type.Heading` on Small), and a one-line description under it (`Type.BodySmall`, `Text.Secondary`), 32 px padding.
- **Hero only:** a "Start" chip bottom-right (A glyph + "Play") that appears on focus.

| State | Image | Label | Frame | Other |
|---|---|---|---|---|
| Default | 100%, desaturated 20%, brightness 0.85 | `Text.Primary` | 1 px `Glass.Edge` | — |
| Focused | Full colour, brightness 1.0, slow 4% push-in over 6 s | `Text.Primary` | Focus ring + glow | Scale 1.03, description fades in to full |
| Pressed | Brightness 1.1 | — | Ring stays | Scale 0.99, 80 ms, then the screen transition |
| Locked | **No image**: a flat `Glass.Fill` panel with a 64 px line icon for the mode (a trophy outline for Career, two linked figures for Team Builder), so no art is commissioned for modes that don't exist | `Text.Secondary` for the label and description (they're meant to be read; ≥ 6:1) | 1 px `Glass.Edge` | Padlock icon 32 px next to the label and the "Coming soon" badge show the locked state. Focused locked: ring shows, fill doesn't raise, description reads why. |

### 6.4 Badge (`UPadelBadge`)

- Pill, 32 px tall, 12 px side padding, `Type.Label`.
- Variants: **Coming soon** (`Surface.Charcoal` fill, `Colour.Lock` text, padlock 16 px), **New** (`Accent` fill, `OnAccent` text), **You** (`Accent` stroke 2 px, `Accent` text; marks the player you control), **Partner** (`Glass.Edge` stroke, `Text.Secondary` text).
- Not focusable.

### 6.5 Profile chip (`UPadelProfileChip`), post-slice

**Not built in the slice**: it does nothing until there are profiles. Kept here so the header leaves room for it later.

- Top right of the main menu header (not top left: the screen title takes the left, and the eye reads the title first).
- 280 × 56 px pill, `Glass.Fill`. A 40 px circle with initials, then the profile name (`Type.Label`). One line only: a second line would need text below the 26 px minimum.
- For the slice the name is the platform user name if available (Steam/Epic later), otherwise "PLAYER 1". Not focusable and does nothing (profiles are post-slice).

### 6.6 Player card (`UPadelPlayerCard`)

Shows one player on the setup screen. **Our own composition, a broadcast "player graphic"**, built like the lower-third name strap a TV match shows when it introduces a player. It deliberately avoids the trading-card layout common in football sims (a big rating stacked over a position code in a corner, portrait in the middle, three-letter stat codes under the name), per the GDD 1 rule on copying UI layouts.

- **Large:** 390 × 560 px in Setup (half the "your pair" panel's inner width, 7.3). **Small:** 321 × 200 px (opponents on setup).
- **Anatomy (large), top to bottom:**
  - **Name strap**, 96 px tall, the full card width, in the team colour at 0.9 alpha: surname (`Type.Title`, `Colour.OnAccent` `#0A1020`, which passes 7:1 on both team colours) with the first name above it (`Type.Label`). A thin 4 px white top edge, like a broadcast strap.
  - **Portrait**, waist up, 280 px tall, standing on the strap's lower edge, left-aligned at 24 px (not centred), with the team name in large outline type behind it (`Type.Display` at 0.08 alpha). Until the asset pipeline renders portraits, a silhouette in the team colour at 0.25 alpha.
  - **Profile line**, right of the portrait, stacked in `Type.BodySmall`, `Text.Secondary`: "Left side", "Right-handed", "1.86 m", and the play style ("Power play"). Words, not codes.
  - **Overall meter**, a bottom band 72 px tall on `Surface.Charcoal`: the word "Overall" (`Type.Body`, `Text.Secondary`) on the left, a 12 px meter across the middle filled to rating/99 in the team colour, the number (`Type.Stat`, tabular) on the right. The rating is one labelled meter among the card's information, not a hero number in a corner.
  - **Four stat rows** above the band: full-word label (`Type.Body`, 160 px wide: Attack, Defence, Net, Movement), a bar (fills to number/99 in `Text.Primary` at 0.85 on a `Glass.Edge` track), the number (`Type.Stat` at 32 px, tabular, right).
  - **Badge**, top-right corner of the strap: You / Partner (6.4).
- **Small card:** the name strap (72 px) and the overall meter only, with no portrait and no stat rows. It sits in a 321 px wide slot.
- **The four card stats** (display only, from `Data/Players.json`, averaged and rounded; the grouping lives in `Data/UiTheme.json` → `PlayerCardStats` so it's tunable text):

| Label | Averages |
|---|---|
| Attack | Overhead, Bandeja, Víbora, Power |
| Defence | Wall Play, Lob, Drive, Backhand |
| Net | Volley, Touch, Anticipation, Positioning |
| Movement | Speed, Acceleration, Agility, Stamina |

- **Play style** comes from the UI roster (`FRosterPlayer::Style`, already in `PadelUiRoster.cpp`), since `Data/Players.json` has no style field yet. Adding one is Agent 3's file and its own PR (W14); the roster table then goes, as decision 0003 already plans for names.
- **Overall:** GDD 6.2 gives the idea (weights depend on preferred side) but not the numbers, and no code computes it (G5). This spec proposes `Padel::Ui::OverallRating(const FPlayerRow&)` in the UI model, reading weights from a new `OverallWeights` block in `Data/Players.json` (Agent 3's file, so its own PR; 13 Q1). Until then the card shows the plain average of all 23 attributes, which gives Arunde 78, Ventara 77, Corvisa 78, Halvaro 77 (inside GDD 6.4's 75–82).

| State | Frame | Other |
|---|---|---|
| Default | `Glass.Fill`, 1 px `Glass.Edge` | — |
| Selected (you control this player) | 2 px `Accent` stroke | "YOU" badge |
| Not selected (your partner) | `Glass.Fill`, 1 px `Glass.Edge` | "PARTNER" badge |

In the slice the cards are **not focusable** (7.3: X swaps from anywhere), so they have no focused or pressed state.

- **Details (post-slice):** a later Y action would open an overlay listing all 23 attributes in GDD 6.1's groups. When it lands, all four cards (including the opponents, the main reason to open it) become focusable and gain the focused state of the tile (ring, scale 1.03).

### 6.7 Toggle (`UPadelToggle`)

- A settings row: label left (`Type.Body`), switch right. Row 72 px tall, full column width.
- Switch: 72 × 36 px pill. **On:** `Accent` fill, `OnAccent` knob on the right, "ON" (`Type.Label`, `OnAccent`) on the left of the knob. **Off:** `Glass.Edge` fill, `Text.Secondary` knob on the left, "OFF" on the right. The word is always shown, so on/off doesn't depend on colour.
- A toggles. Left = Off, Right = On (pressing toward the side you want), which plays an edge sound if it's already there.
- Row states as the list button (6.2): focused adds the left accent bar and raised fill. Disabled greys the whole row.

### 6.8 Selector (`UPadelSelector`)

The left/right value picker, the main control on setup and in settings ("Difficulty  ‹ MEDIUM ›").

- Row 72 px; label left (`Type.Body`), value block right: `‹` (32 px), value (`Type.Heading`, centred in a 240 px box), `›`. Under the value, dots, one per option, the current one in `Accent` (only when there are 2 to 5 options).
- Optional description line below the row, in `Text.Secondary`, shown only when focused ("AI reads your shots later and misses more.").
- Left/right steps; **no wrap** at the ends (the arrow on that side greys to `Text.Disabled`), so holding a direction can't loop past the value you wanted. A on the row steps forward and **does** wrap, so mouse users and one-button users can still reach every value.
- Value change: the old value slides out 24 px toward the arrow pressed and the new one slides in from the other side, 140 ms (section 9).

| State | Row | Arrows |
|---|---|---|
| Default | transparent | `Text.Secondary` |
| Focused | `Glass.FillRaised`, left accent bar | `Accent` |
| Pressed (arrow) | — | Pressed arrow scales 0.85 for 80 ms |
| At an end | — | That arrow `Text.Disabled` |
| Disabled | `Text.Disabled` everywhere | hidden |

### 6.9 Segmented control (`UPadelSegmented`)

For two or three short options shown side by side (match format on setup: "1 SET | BEST OF 3").

- Pill, 56 px tall, each segment min 160 px. Selected segment: `Accent` fill, `OnAccent` text. Others: transparent, `Text.Secondary`.
- Focus is on the whole control (ring around it); left/right moves the selection directly (no separate confirm). A does nothing extra.

### 6.10 Slider (`UPadelSlider`)

Volumes and similar values.

- Row 72 px: label left, track 360 × 6 px, knob 24 px circle, value number right (`Type.Stat` at 32 px, tabular, 64 px box, "0" to "100").
- Filled part `Accent` when focused, `Text.Primary` at 0.6 otherwise. Knob `Text.Primary`; focused knob gets the glow.
- Left/right steps by 5 (keyboard and pad); holding accelerates (5.3). Mouse drag sets it directly.

### 6.11 Tabs (`UPadelTabBar`)

- A row of tab labels under the screen title, `Type.Heading`, 48 px gap, flanked by the LB and RB glyphs (32 px).
- Selected tab: `Text.Primary` with a 4 px `Accent` underline spanning the full label width, so it's easy to see from the couch.
- Other tabs: `Text.Secondary`. Tabs aren't in the focus map; only LB/RB (or Q/E, or a click) change them. Changing tab resets focus to the first element of the new tab. Tabs don't wrap.

### 6.12 Modal dialog (`UPadelDialog`)

- Over everything, with an extra `Bg.Scrim` at 0.6. Panel 6 columns wide (852 px), height to content, `Surface.Charcoal` (solid, not glass, so the question reads), `Radius.Panel`.
- Title (`Type.Title`), body (`Type.Body`, `Text.Secondary`, max 3 lines), then buttons side by side, right-aligned: the **safe** option on the right and focused by default, the action option on its left. For a destructive action the action button is the Danger variant (6.2).
- B always picks the safe option. A dialog can't be dismissed by Start.
- Opens with a 160 ms fade and scale 0.96 → 1.0; closes in 100 ms.

### 6.13 Toast (`UPadelToast`)

- A small glass panel, 480 px wide, bottom-centre above the hint bar, one line `Type.BodySmall`. Shows for 2.5 s, fades 200 ms. Not focusable. One at a time; a new one replaces the old.
- Used for: locked tile pressed ("Career is coming in a later update."), settings saved, controller connected or disconnected.

### 6.14 Screen header (`UPadelScreenHeader`)

- The header band (4.3): screen title `Type.Display` left; optional breadcrumb above it in `Type.Label`, `Text.Secondary` ("PAUSE  /  SETTINGS"); the right side holds the profile chip (post-slice, main menu), the match score line (Pause) or nothing.

### 6.15 Background (`APadelMenuBackdrop`, an actor, not a widget)

Slice must: one fixed camera shot of the court per screen family. Slice if time: the slow paths below.

- The menus render over the court level with a slow camera dolly: a looping 40 s path around the outside of the glass at 2–4 m height, one move per screen family (Title: low wide orbit; Main: high slow push along the side glass; Setup: behind the back glass looking down the court, like the broadcast camera). The path points live in `Data/Camera.json` → `MenuShots`.
- Optional idle players hitting soft balls on court (post-slice; for the slice the court is empty, with arena lights and a little haze).
- **Reduce motion** (7.7) freezes the camera on the first frame of each path.
- If the court level isn't loaded (boot), `Bg.Deep` with the vignette.

### 6.16 Loading indicator

- A padel ball icon (20 px, `Text.Primary`) bouncing on a short line, 600 ms loop, bottom-right of the safe area, with "Loading" in `Type.Label`. Shown only if a load takes over 300 ms.

---

## 7. Screens

Positions are in grid columns (C1–C12) inside the safe area, and pixel rows from the top of the screen at 1080p. Every screen has the header band at y 64–160 and the hint bar band at y 960–1016 unless it says otherwise.

### 7.1 Title

**Purpose:** first impression and "press any button", so the game knows which device the player is using.

**Entry:** game boot, after the engine splash. **Exit:** any button → main menu.

**Reading order:** the logo, then the prompt. Nothing else competes.

**Layout:**

| Element | Position | Spec |
|---|---|---|
| Backdrop | full screen | Title camera path (6.15), scrim at 0.35 only (lighter than other screens), vignette on. |
| Logo text "PADEL STREET" | centred, y 360 | `Type.Hero`, `Text.Primary`, with a 6 px lime underline stroke under "STREET" that draws left to right in 400 ms on first show. A working-title wordmark in our font, not a logo asset (GDD 13 decision 2). |
| Tagline | centred, y 492 | `Type.Heading`, `Text.Secondary`: "2V2 PADEL" |
| Prompt | centred, y 820 | `Type.Heading`, `Text.Primary`: "Press any button" (pad connected) or "Press any key" (no pad). Pulses alpha 1.0 ↔ 0.5, 1.6 s loop (steady if Reduce motion). |
| Build line | bottom-left of safe area | `Type.BodySmall`, `Text.Disabled`: "Vertical slice · build {version}". |

**No hint bar.** **Focusable elements:** none; the whole screen listens for any key or button (but not stick movement or mouse movement, which are too easy to trigger by accident).

**Transition out:** prompt flashes `Accent` 100 ms, logo moves up and fades over 240 ms as the main menu's tiles slide in (9).

### 7.2 Main menu

**Purpose:** pick a mode. **Entry:** from Title, Setup (B), Stats, Pause (Quit), Practice (Quit). **Default focus:** Quick Match tile. **Exit:** a tile, or B → Quit game? dialog.

**Reading order:** the Quick Match hero tile (the one primary action, largest and focused), then Practice and Settings, then the bottom row. Six focusable items.

**Layout (1080p):**

```
y 64   ┌──────────────────────────────────────────────────────────────────────┐
       │ PADEL STREET                                                         │  header
y 160  ├──────────────────────────────────────────────────────────────────────┤
y 184  │ ┌───────────────────────────────────────────┐ ┌─────────────────────┐ │
       │ │                                           │ │ PRACTICE            │ │
       │ │                                           │ │ Ball machine drills │ │  y 184–432
       │ │            QUICK MATCH (hero)             │ └─────────────────────┘ │
       │ │                                           │ ┌─────────────────────┐ │
       │ │  You and an AI partner against two AI.    │ │ SETTINGS            │ │  y 456–704
       │ │                                [A] Play   │ └─────────────────────┘ │
       │ └───────────────────────────────────────────┘                         │  hero y 184–704
y 728  │ ┌─────────────────────┐ ┌─────────────────────┐ ┌─────────────────────┐ │
       │ │ CAREER        🔒    │ │ TEAM BUILDER  🔒    │ │ CONTROLS            │ │  y 728–936
       │ └─────────────────────┘ └─────────────────────┘ └─────────────────────┘ │
y 960  │                                                 [A] Select  [B] Quit │  hint bar
       └──────────────────────────────────────────────────────────────────────┘
```

The featured player from the brief is the hero tile's image (Ventara and Arunde, Team Lagoon, mid-rally), not a separate panel: a separate panel on the right would push the tiles into too little width at 1080p and isn't focusable, so it's only decoration taking space.

**Elements:**

| Id | Component | Position | Label | Description (focused) | Image | A does | States |
|---|---|---|---|---|---|---|---|
| `Main.QuickMatch` | Tile, Hero | C1–C8, y 184–704 | QUICK MATCH | You and an AI partner against two AI players. | Team Lagoon mid-rally at the net | Match setup (7.3) | Default, Focused, Pressed |
| `Main.Practice` | Tile, Small | C9–C12, y 184–432 | PRACTICE | Ball machine drills for every shot. | Ball machine and one player on court | Practice (7.9) | Default, Focused, Pressed |
| `Main.Settings` | Tile, Small | C9–C12, y 456–704 | SETTINGS | Audio and accessibility. | Close-up of the court glass and frame | Settings (7.7), opener = Main | Default, Focused, Pressed |
| `Main.Career` | Tile, Small (Locked) | C1–C4, y 728–936 (208 px tall) | CAREER | Coming in a later update. | None: flat glass + trophy line icon (6.3) | Toast "Career is coming in a later update." | Locked (+ focused) |
| `Main.TeamBuilder` | Tile, Small (Locked) | C5–C8, y 728–936 | TEAM BUILDER | Coming in a later update. | None: flat glass + linked-figures line icon (6.3) | Toast "Team Builder is coming in a later update." | Locked (+ focused) |
| `Main.Controls` | Tile, Small | C9–C12, y 728–936 | CONTROLS | See every button. | Gamepad on the bench | Controls (7.6), opener = Main | Default, Focused, Pressed |

The bottom row is 208 px tall, shorter than the 248 px Small tiles above it, so the Small tile is specified with a height token, not a fixed size.

**Focus map:**

| From | Up | Down | Left | Right |
|---|---|---|---|---|
| QuickMatch | — | Career | — | Practice |
| Practice | — | Settings | QuickMatch | — |
| Settings | Practice | Controls | QuickMatch | — |
| Career | QuickMatch | — | — | TeamBuilder |
| TeamBuilder | QuickMatch | — | Career | Controls |
| Controls | Settings | — | TeamBuilder | — |

**Hint bar:** A Select, B Quit.

**Should locked tiles be shown at all?** Showing Career and Team Builder sets the expectation for the full game, which Caio's brief asked for. They're focusable but can't be entered, so the slice never presents something that pretends to work. 13 Q2 asks whether to show them in the playtest build.

**Transitions:** on entry from Title, the hero tile slides in from 48 px left and fades (240 ms), the small tiles follow staggered 40 ms each. On A, the pressed tile scales to 0.99, then the whole menu fades and slides 32 px left (200 ms) as the next screen slides in from the right.

### 7.3 Match setup (Quick Match)

**Purpose:** set up the match and start it. GDD 11 mode 1 options: difficulty, match length, golden point, which player the human controls. **Entry:** Main.QuickMatch. **Default focus:** `Setup.Start`, so the defaults (you play Ventara, GDD 6.4; Medium; best of 3; golden point on) start with one press. **Exit:** Start match → Versus; B → Main menu.

**Reading order:** the matchup first (your pair's cards, then the opponents, in team colours), then the rules, then Start match. The matchup is the hero; options are secondary.


**What can actually be chosen in the slice.** The slice has one pair per side: the human always plays for Team Lagoon (Team A, `FMatchSetup::HumanPlayerId` is 0 or 1), with the other Lagoon player as the AI partner, against Team Ember. So "choose your player and partner" is one choice (which Lagoon player you are; the partner is the other), and the opponents are shown but not chosen. Choosing teams and partners is post-slice (13 Q5).

**Layout:**

```
y 64   QUICK MATCH                                         LAGOON  vs  EMBER
y 160  ───────────────────────────────────────────────────────────────────────
y 184  ┌─────────── YOUR PAIR · LAGOON ───────────┐     ┌─ OPPONENTS · EMBER ─┐
       │ ┌──────────────────┐ ┌──────────────────┐ │     │┌────────┐┌────────┐│
       │ │▀ MIKKO     [YOU] │ │▀ SELVA [PARTNER] │ │     ││▀CORVISA││▀HALVARO││
       │ │  VENTARA         │ │  ARUNDE          │ │     ││Ovr ▬▬ 78││Ovr ▬▬ 77││
       │ │ [portrait] Left  │ │ [portrait] Right │ │ VS  │└────────┘└────────┘│
       │ │            side  │ │            side  │ │     ├─── MATCH RULES ────┤
       │ │ Attack   ███  83 │ │ Attack   ██   72 │ │     │ Difficulty ‹MEDIUM›│
       │ │ Defence  ██   74 │ │ Defence  ███  81 │ │     │ Format [1 SET|BO3] │
       │ │ Net      ██   75 │ │ Net      ██   78 │ │     │ Golden point [ON]  │
       │ │ Movement ██   76 │ │ Movement ███  80 │ │     │                    │
       │ │ Overall ▬▬▬▬  77 │ │ Overall ▬▬▬▬  78 │ │     │ [  START MATCH   ] │
       │ └──────────────────┘ └──────────────────┘ │     └────────────────────┘
y 936  └──────────────────────────────────────────┘
y 960  [A] Start match  [X] Swap player  [B] Back
```

| Region | Columns / size | Rows |
|---|---|---|
| Your pair panel, title "YOUR PAIR · LAGOON" in `Team.Lagoon` | C1–C6 (852 px) | y 184–936 |
| Two large player cards inside it, 24 px panel padding, 24 px gap: (852 − 48 − 24) / 2 = **390 px** each | x +24 and +438 within the panel | y 256–816 |
| "VS" mark | C7, centred | y 520, `Type.Display`, `Text.Secondary` |
| Opponents panel, title "OPPONENTS · EMBER" in `Team.Ember` | C8–C12 (706 px) | y 184–448 |
| Two small cards inside it, 24 px padding, 16 px gap: (706 − 48 − 16) / 2 = **321 px** each | | y 232–432 (200 px) |
| Match rules panel, title "MATCH RULES" | C8–C12 | y 472–936 |
| Start match (primary button), panel width minus padding (658 × 64) | C8–C12 | y 848–912 |

The header right shows "LAGOON  vs  EMBER" in team colours (`Type.Heading`).

**Elements:**

| Id | Component | Label / content | Focusable | A does | Left/Right | States |
|---|---|---|---|---|---|---|
| `Setup.YouCard`, `Setup.PartnerCard` | Player card, large | The player you control (default Mikko Ventara) with "YOU", and your AI partner with "PARTNER" | No | — | — | Selected / Not selected |
| `Setup.Opponent1`, `Setup.Opponent2` | Player card, small | Ilan Corvisa, Ruben Halvaro | No | — | — | Default |
| `Setup.Difficulty` | Selector | "Difficulty" · EASY / MEDIUM / HARD (default MEDIUM) | Yes | Steps forward, wraps | Steps, no wrap | Default / Focused / At an end |
| `Setup.Format` | Segmented | "Format" · 1 SET / BEST OF 3 (default BEST OF 3, GDD 13 decision 5) | Yes | — | Selects | Default / Focused |
| `Setup.GoldenPoint` | Toggle | "Golden point" · ON / OFF (default ON) | Yes | Toggles | Left Off, Right On | Default / Focused |
| `Setup.Start` | Primary button | "START MATCH" with A glyph | Yes (default) | Starts: Versus (7.4) if built, else straight to the match | — | Default / Focused / Pressed |

**Swap player (X, from anywhere on the screen).** X swaps which Lagoon player you control (`FMatchSetup::CyclePlayer`), whatever is focused, and is always in the hint bar. So the cards never need focus and there's no navigation trap between the cards and the rules. The cards don't move: the "YOU" and "PARTNER" badges cross-fade (160 ms) and the Selected stroke moves to the other card. Keyboard: R. Mouse: click either large card.

**Descriptions, shown under the focused rules row (`Type.BodySmall`, `Text.Secondary`):**

| Row | Value | Description |
|---|---|---|
| Difficulty | Easy | The AI reads your shots late and plays it safe. |
| | Medium | A fair match. The AI plays smart padel and makes some mistakes. |
| | Hard | The AI reads the ball early, attacks the net and rarely misses. |
| Format | 1 set | First to 6 games, by two clear. Tie-break at 6–6. |
| | Best of 3 | First to win 2 sets. Each set is first to 6 games, by two clear, with a tie-break at 6–6. |
| Golden point | On | At deuce, one deciding point. The receiving pair picks who receives. |
| | Off | Advantage scoring: win by two points from deuce. |

No match-length times are shown. If they're added later, they must come from the soak test's measured average, not guesses (13 Q6).

The descriptions must match the GDD 3.1 rules (tie-break at 6–6, golden point receiver choice). Agent 2 checks this copy against the referee.

**Focus map:** four focusable elements in one vertical list, wrapping. Left/right belong to the focused control's value.

| From | Up | Down | Left | Right |
|---|---|---|---|---|
| Difficulty | Start (wrap) | Format | value | value |
| Format | Difficulty | GoldenPoint | value | value |
| GoldenPoint | Format | Start | value | value |
| Start (default) | GoldenPoint | Difficulty (wrap) | — | — |

**Hint bar:** A Start match (or A Change on a rules row) · X Swap player · B Back.

**Edge cases:**

- Setup choices persist for the session (`APadelHUD::Setup` already does) and are saved to the local settings file so they survive a restart (11.3).
- If `Data/Players.json` lacks a player, the card shows "Player {id}" (`DisplayName` fallback) and "--" for stats, and Start stays enabled.
- `FMatchSetup::DifficultyKey()` returns "easy/medium/hard" but the rows in `Data/Difficulty.json` are named "Easy/Medium/Hard". The match owner must match them case-insensitively, or one side changes (a gap for Agent 4, not a UI change).

### 7.4 Versus (pre-match card), slice if time

**Purpose:** a short broadcast-style beat between setup and the first serve, and time to load the match. **Entry:** Setup.Start, Pause.Restart (confirmed), Stats.Rematch. **Exit:** automatically after 2.5 s, or A to skip once the match is ready.

**Reading order:** the two team names, then the players, then the rules line.

| Element | Position | Spec |
|---|---|---|
| Backdrop | full | Broadcast camera (GDD 9) slowly settling into its match position. |
| Left block, Lagoon | C1–C5, y 380–700 | Team bar 8 px `Team.Lagoon`; "LAGOON" `Type.Display`; the two players as "VENTARA · ARUNDE" `Type.Title`, the human's name first with a "YOU" badge. |
| "VS" | centre, y 520 | `Type.Hero` at 96 px, `Text.Secondary`. |
| Right block, Ember | C8–C12, right-aligned | Mirror of left, `Team.Ember`. |
| Rules line | centred, y 760 | `Type.Heading`, `Text.Secondary`: "MEDIUM · BEST OF 3 · GOLDEN POINT". |
| Loading indicator | bottom-right | Only if the match isn't ready at 2.5 s; the card stays until it is. |

**Hint bar:** A Skip (only once the match is ready).

**Transition:** blocks slide in from their sides (240 ms), "VS" scales 1.2 → 1.0 (200 ms). Out: everything fades 200 ms while the HUD score bug fades in.

**Gap:** today `StartMatch()` goes straight to the match. The Versus screen needs the match owner to say "ready" (an `OnMatchReady` call, or `MatchEvents.HasLiveMatch()` turning true), which is a contract with Agent 2 (11.4 W5).

### 7.5 Pause

**Purpose:** stop the match, see the score, change settings, leave. **Entry:** Start (or Esc) during a match. **Default focus:** Resume. **Exit:** Resume (A on Resume, B, or Start) → Match.

**Reading order:** the score line, then the menu column, then the context panel.

The game is paused (`SetPause(true)`, as today) and the match stays visible behind a 0.70 scrim, so the player keeps their place.

**Layout:**

| Element | Position | Spec |
|---|---|---|
| Scrim | full | `Bg.Scrim` at 0.70 over the screen's single blur layer (4.3); no blur if Reduce transparency. |
| Header | header band | Title "PAUSED" (`Type.Display`); right side: the score line (`Type.Heading`, tabular), for example "LAGOON 6–4  3–2  30–15  EMBER", from `Padel::Ui::SummaryLine(LastScore)`, with team names in team colours and the serving team marked with a small ball icon in `Text.Primary`. |
| Menu column | C1–C4, from y 216 | Glass panel, list buttons (6.2), 72 px each, 8 px apart. |
| Context panel | C6–C12, y 216–760 | Shows what the focused item does, for the slice: on Resume and Restart, the match rules ("MEDIUM · BEST OF 3 · GOLDEN POINT") and the two pairs; on Controls, a small version of the gamepad diagram (7.6); on Settings, "Audio and accessibility"; on Quit, "The match will not be saved." Not focusable. |
| Hint bar | bottom, on its charcoal strip | A Select · B Resume |

**Elements:**

| Id | Label | A does | States |
|---|---|---|---|
| `Pause.Resume` | RESUME | Back to the match | Default / Focused / Pressed |
| `Pause.Restart` | RESTART MATCH | Dialog "Restart match?" (7.10) → Versus | Default / Focused / Pressed |
| `Pause.Controls` | CONTROLS | Controls (7.6), opener = Pause | Default / Focused / Pressed |
| `Pause.Settings` | SETTINGS | Settings (7.7), opener = Pause | Default / Focused / Pressed |
| `Pause.Quit` | QUIT TO MAIN MENU | Dialog "Quit to main menu?" (7.10) → Main menu | Default / Focused / Pressed |

**Focus map:** a vertical list, Resume → Restart → Controls → Settings → Quit, wrapping. Left/right do nothing.

**Rules:**

- **Pause can't open** while the golden point receiver prompt is up (the HUD owns that input), during the Versus card, or during the 4 s end-of-match card. Pressing Start then is ignored.
- A controller disconnecting during a match opens Pause automatically, with a toast "Controller disconnected. Reconnect to continue." (10). Resume is disabled until a pad or keyboard input arrives.
- Changing a setting from Pause applies immediately (7.7); there's no "apply" step to forget.

**Gap:** today's pause has Resume, Restart and Quit only, and Restart and Quit act without a confirm (G9).

### 7.6 Controls

**Purpose:** show every match action and its button (GDD 7), on the device in use. Read-only in the slice (rebinding is post-slice, GDD 7). **Entry:** Main.Controls or Pause.Controls. **Exit:** B → opener.

**Tabs (6.11):** GAMEPAD · KEYBOARD. Opens on the tab matching the most recent device.

**Gamepad tab layout:**

| Element | Position | Spec |
|---|---|---|
| Pad diagram | C3–C10, y 232–760 | A neutral, original line drawing of a generic two-stick pad (not any manufacturer's outline), `Text.Secondary` 2 px strokes, built as SVG by the pipeline. Leader lines from each control to its label. |
| Labels | around the diagram | `Type.Body`, `Text.Primary`, each with its glyph. Content below. |
| Footer note | y 800, centred | `Type.BodySmall`, `Text.Secondary`: "Hold a shot button to charge it. Release to swing. Aim with the left stick when you release." (GDD 7 Aim and Power) |

**Labels (from GDD 7; post-slice actions are shown greyed with "Later"):**

| Control | Label |
|---|---|
| Left stick | Move · aim |
| RT | Sprint |
| A | Drive · volley · serve |
| B | Lob |
| Y | Overhead (bandeja, smash) |
| X | Drop shot — *Later* (`Text.Disabled`) |
| RB | Attack modifier (víbora, kick serve) |
| LB | Touch modifier (chiquita) |
| D-pad up | Partner: go to the net |
| D-pad down | Partner: stay back |
| D-pad left | Partner: take the middle |
| D-pad right | Partner: yours |
| Start | Pause |

**Keyboard tab:** a two-column table (Action · Key cap), same rows and order, keys from the GDD 7 keyboard column (WASD, Shift, J, K, L, I, E, Q, 1–4, Esc).

**Labels must come from the live bindings**, not be typed into the screen: read `UPadelInputConfig`'s `IMC_Match` mappings (as `APadelHUD::ButtonLabel` does) and label them by action. Then the screen stays right if a binding changes.

**Focus:** there's nothing to focus on this screen. The screen itself holds focus so LB/RB and B work; no focus ring is shown.

**Hint bar:** LB RB Change tab · B Back.

**In practice:** the D-pad rows read "Practice: change drill / pause machine / feed a ball" instead (`APadelPracticeDirector`'s controls), when opened from Practice pause.

### 7.7 Settings

**Purpose:** the few settings the slice needs, and no more. Every row needs a working feature behind it, so each one names its hook and owner. **Reading order:** the tab, the focused row, its description. **Entry:** Main.Settings or Pause.Settings. **Exit:** B → opener.

**Every change applies immediately** and is saved when leaving the screen (`UPadelSettings`, a `UGameUserSettings` subclass, 11.3). There's no Apply button.

**Layout:**

| Element | Position | Spec |
|---|---|---|
| Header | header band | "SETTINGS", breadcrumb "PAUSE / SETTINGS" when opened from Pause. |
| Tab bar | y 176 | AUDIO · ACCESSIBILITY in the slice (GAMEPLAY is added if time allows). |
| Settings list | C1–C7, y 248–936 | Glass panel; rows 72 px with 8 px gaps. |
| Description panel | C8–C12, y 248–600 | The focused row's name (`Type.Title`) and description (`Type.Body`, `Text.Secondary`). |
| Hint bar | | A Change (toggles and selectors) · X Reset (only if anything on the tab isn't default) · LB RB Change tab · B Back |

The X reset asks first: dialog "Reset {tab} settings?" (7.10).

**Slice: must**

*Audio tab:*

| Id | Control | Label | Values (default **bold**) | Description | Hook and owner |
|---|---|---|---|---|---|
| `Set.Vol.Master` | Slider | Master volume | 0–100, **80** | — | A `USoundMix` with one `USoundClass` per row below, pushed at start; the slider sets the class volume. Presentation (`UPadelMatchAudio` already routes every match sound), W15. |
| `Set.Vol.Effects` | Slider | Ball and court | 0–100, **100** | Racket, glass, mesh and footsteps. These help you read the point. | Same mix, class `Effects`. |
| `Set.Vol.Crowd` | Slider | Crowd | 0–100, **70** | — | Same mix, class `Crowd` (`CrowdPoint`, `CrowdBigPoint`). |
| `Set.Vol.Ui` | Slider | Menu sounds | 0–100, **70** | — | Same mix, class `Ui`; only once the menu sounds exist (W10), otherwise the row is left out. |

A sound plays at the new volume each time an audio slider changes (from its own class), so the player can hear the level. A **Menu music** slider is added only if a menu music loop is added; none exists today.

*Accessibility tab:*

| Id | Control | Label | Values (default **bold**) | Description | Hook and owner |
|---|---|---|---|---|---|
| `Set.TextSize` | Selector | Text size | **Default** / Large / Largest | Makes text bigger in menus and the HUD. | Multiplies the type tokens (4.2) in `FPadelUiTheme`; screens rebuild on change. UI. |
| `Set.ReduceMotion` | Toggle | Reduce motion | **Off** / On | Stops the menu camera, tile push-ins, pulses and slides; screens cross-fade instead. | `FPadelUiTheme::bReduceMotion`, read by `UPadelScreenWidget` and the backdrop. UI and Presentation. |

**Slice: if time**

| Id | Tab | Control | Label | Values | Description | Hook and owner |
|---|---|---|---|---|---|---|
| `Set.TimingCue` | Gameplay | Toggle | Timing cue | **On** / Off | Shows Early, Good, Perfect or Late next to your player after each of your shots. | Gate the `EPadelHudMessage::TimingCue` call in `APadelHUD::OnBallHit`. UI. |
| `Set.PartnerCallouts` | Gameplay | Toggle | Partner command prompts | **On** / Off | Shows the command you gave your partner at the bottom left. | Gate `APadelHUD::ShowPartnerCommand`. UI. |
| `Set.ReduceTransparency` | Accessibility | Toggle | Reduce transparency | **Off** / On | Solid panels instead of glass. | Swaps `Glass.*` for `Surface.Charcoal` and drops the blur layer (4.3). UI. |
| `Set.TeamColours` | Accessibility | Selector | Team colours | **Lagoon and Ember** / High contrast | High contrast swaps the teams to blue `#3DA5FF` and pink `#FF5FA2`, checked for deuteranopia, protanopia and tritanopia. Yellow is avoided because it fights the ball. | `Team.*` tokens; HUD, cards and stats read them. UI. |

**Post-slice** (not built, not shown): a Display tab (window mode, resolution with a revert dialog, V-sync, frame limit, graphics quality; the engine defaults and `GameUserSettings.ini` cover the slice), Brightness, Vibration and its strength (nothing in `Source/` plays force feedback, and the GDD doesn't mention rumble; it needs a Gameplay feature first), Stick dead zone (GDD 7 puts dead-zone settings post-slice), Hold to confirm, and rebinding.

**Focus map:** a vertical list per tab, wrapping. Left/right change the focused control's value. A toggles a toggle, steps a selector forward (wrapping), and does nothing on a slider.

### 7.8 Post-match stats

**Purpose:** the result and how it happened. **Reading order:** the winner title, then the set scores, then the stat rows. **Entry:** 4 s after `OnMatchWon` (`MatchEndDelaySeconds`, today). **Default focus:** Rematch. **Exit:** Rematch → Versus; Main menu → Main menu. B does nothing here (section 3), so the stats can't be lost to a stray press; today B goes straight to the main menu.

**Layout:**

```
y 64   LAGOON WIN                                                     MATCH STATS
y 160  ───────────────────────────────────────────────────────────────────────────
y 184  ┌──────────────────────────────── SCORE ─────────────────────────────────┐
       │  LAGOON  VENTARA · ARUNDE        6     3     7        ✓                 │
       │  EMBER   CORVISA · HALVARO       4     6     5                          │  y 184–400
       └────────────────────────────────────────────────────────────────────────┘
       ┌────────────── SUMMARY | SHOTS | POINTS (tabs) ─────────────────────────┐
       │                    LAGOON                         EMBER                 │
       │  Points won          68   ██████████▌│███████▌        57                │
       │  Winners             21   ██████▌    │█████           16                │  y 424–848
       │  Errors              14   ████▌      │██████          19                │
       │  Smashes              9   ███        │██▌              7                │
       │  Points won off the glass  ...                                          │
       └────────────────────────────────────────────────────────────────────────┘
y 872  Average rally 8.4 shots · 31% of points…          [ REMATCH ]  [ MAIN MENU ]
y 960                                           [A] Select   [LB RB] Change tab
```

**Header:** title "{TEAM} WIN" (`Type.Display`, in the winning team's colour) or "MATCH STATS" if the match was abandoned (not reachable in the slice, since Quit skips stats). Right side "MATCH STATS" in `Type.Label`.

**Score panel (C1–C12, y 184–400):** two rows, one per team. Team name (`Type.Title`, team colour), pair surnames (`Type.Body`, `Text.Secondary`), then one column per set (`Type.Score`, tabular), the winner of each set at full `Text.Primary` and the loser at `Text.Secondary`. A tie-break set shows the tie-break score as a superscript ("7⁵" style: `Type.Label` raised 32 px). A check mark in the winning team's colour after the winning team. Data: `FMatchStats::FinalSets`; tie-break points need adding to the stats model (W8).

**Stats panel (C1–C12, y 424–848), three tabs:**

Each row: label (`Type.Body`, `Text.Secondary`, left, C1–C4), Team A number (`Type.Stat`, right-aligned at C5), a centred comparison bar (C6–C9: Team A grows left from the centre in `Team.Lagoon`, Team B right in `Team.Ember`, both scaled to the larger of the two; 16 px tall), Team B number (C10, left-aligned). The higher number in each row is `Text.Primary`, the lower `Text.Secondary`, except for Errors and Double faults where lower is better and the highlight flips.

| Tab | Rows | Source |
|---|---|---|
| SUMMARY | Points won · Winners · Errors · Smashes · Points won off the glass · Double faults | `FMatchStats::PointsWon`, `Winners`, `Errors`, `Shots["smash"]`, *new* glass-points-won count (W8), `DoubleFaults` |
| SHOTS | One row per shot type that was played: Drive, Volley, Lob, Bandeja, Víbora, Smash, Chiquita, Serve | `FMatchStats::Shots`, labelled by `PadelUiCodes`, in GDD 4 order |
| POINTS | "How the points were won", one row per reason code, most common first | `FMatchStats::EndingRows()` |

The brief's "points won off the glass" isn't the same as today's "shots off the glass" (`OffGlassShots`): it's points won by the team where that team played a shot off the glass. The stats model needs a per-team count of points won with an off-glass shot by the winning team (W8). The summary keeps both: "Shots off the glass" stays in the SHOTS tab.

**Footer line (y 872–936, left of the buttons, vertically centred):** "Average rally {n} shots · {p}% of points had a shot off the glass" (`Type.BodySmall`, `Text.Secondary`, left), from `AverageRally()` and `GlassPointShare()`.

**Buttons (bottom right, C8–C12, y 872–936, the last 64 px of the content area):**

| Id | Component | Label | A does |
|---|---|---|---|
| `Stats.Rematch` | Primary button (320 × 64) | REMATCH | Same setup, Versus → match (today's "Play again") |
| `Stats.MainMenu` | Secondary button (6.2, 240 × 64) | MAIN MENU | Main menu |

The hint bar stays in its own band at y 960.

**Stray-press lock.** A is also the drive, volley and serve button, so a player still pressing A at match point would land here (4 s after `OnMatchWon`) with focus on Rematch, and the 120 ms entry lock is too short to stop that. So on Stats:

- Confirm is **ignored until the entry animation has finished and at least 1.0 s has passed** since the screen appeared (`StatsConfirmLockSeconds` in `Data/UiTheme.json`).
- An A press during the animation only **finishes the animation** (jumps to its end state); it never activates a button.
- A held across the screen change does nothing: confirm needs a fresh press that started after the lock ended.
- The buttons show their Disabled look during the lock and switch to Default/Focused as it ends, so the player can see when they're live.

**Focus map:** Rematch ↔ Main menu (left/right, no wrap); up/down do nothing. The stats panel isn't focusable; LB/RB change its tab. If a tab overflows (SHOTS with eight rows fits; POINTS can have up to ~10 reasons), the right stick scrolls it.

**Hint bar:** A Select · LB RB Change tab. (No B hint, since B does nothing here.)

**Entry animation:** the score rows count up set by set (each set column fades in 120 ms apart), then the stat bars grow from the centre over 400 ms, staggered 40 ms per row. A finishes the animation at once (and doesn't press anything; see the stray-press lock above). Reduce motion shows the end state at once, and the 1.0 s lock still applies.

### 7.9 Practice

**Purpose:** reach the ball machine. **Why it's in the slice:** GDD 11 lists practice under mode 2, but GDD 12's definition of done (item 3) says every slice shot is "verified against the ball machine in practice", and `APadelPracticeDirector` already exists for it. This screen is the smallest way to reach it from the menu. If Caio wants the slice menu to be Quick Match only, the Practice tile becomes Locked like Career and practice stays a dev-only map (13 Q10). **Entry:** Main.Practice. The practice level loads (Loading indicator), then a short overlay explains the controls.

**Practice intro overlay** (shows on entry, A to dismiss, then the practice starts):

| Element | Spec |
|---|---|
| Panel | C3–C10, centred, glass. |
| Title | "PRACTICE" `Type.Display`. |
| Body | "The ball machine feeds you balls. Play any shot, including off the glass." |
| Control list | Four rows with glyphs, from `APadelPracticeDirector`: "D-pad ◁ ▷  Change drill", "D-pad △  Pause the machine", "D-pad ▽  Feed a ball now", "Start  Pause". |
| Primary button | "START PRACTICE" |
| Hint bar | A Start · B Back (returns to the main menu) |

**Practice pause** (Start in practice): the Pause layout (7.5) with the list RESUME · CONTROLS · SETTINGS · QUIT PRACTICE. No Restart, no score line; the header right shows the current drill name instead. Quit practice doesn't need a confirm (nothing is lost).

**Gap:** there's no menu path to the practice director today, and the director's drill names aren't exposed for the UI (W9).

### 7.10 Confirm dialogs

All use the modal dialog (6.12). Safe option focused by default and on the right.

| Id | Title | Body | Action button | Safe button | Opened from |
|---|---|---|---|---|---|
| `Dlg.QuitGame` | Quit Padel Street? | — | QUIT GAME (Danger) | CANCEL | Main menu B |
| `Dlg.QuitMatch` | Quit to main menu? | The match will not be saved. | QUIT MATCH (Danger) | KEEP PLAYING | Pause.Quit |
| `Dlg.Restart` | Restart the match? | The score goes back to 0–0. | RESTART | KEEP PLAYING | Pause.Restart |
| `Dlg.ResetTab` | Reset {tab} settings? | All {tab} settings go back to their defaults. | RESET | CANCEL | Settings X |


### 7.11 HUD alignment (not redesigned here)

The in-match HUD keeps its layout. To match the menus it only:

- Uses the Barlow fonts and tabular figures for the score bug and score card.
- Uses `Team.*` and `Text.*` tokens from the theme file instead of `PadelUiStyle` constants.
- Keeps its yellow callout accent until the HUD pass (lime is menus only, 4.1; 13 Q4).
- The golden point receiver prompt (GDD 3.1, GDD 7 "stick + A") uses the dialog look (6.12) with the two Lagoon player names as buttons and a hint bar "◁ ▷ Choose · A Confirm". It opens focused on the human's own player (today's `OpenGoldenPrompt(Setup.HumanPlayerId, …)`), so A alone always works. It needs the same stray-press rule as Stats: the player is likely mid-swing on A when deuce arrives, so the prompt ignores A for 0.5 s after it opens and needs a fresh press (HUD pass). There's no timeout: the match is held while it's open (as `APadelHUD::Tick` does today), and Pause can't open over it.

### 7.12 Edge and error states

| Case | What the player sees |
|---|---|
| `Data/UiTheme.json` missing or invalid | Built-in default tokens (the values in section 4), and one `LogPadel` warning. Never a blank screen. |
| `Data/Players.json` missing a player | Card shows "Player {id}" and "--" stats (7.3); Start stays enabled. |
| Tile or portrait image missing | The tile shows a flat `Glass.Fill` with the court line icon; the card shows the team-colour silhouette. |
| Very long names (localised or future players) | Names shrink to 85% of their size once, then truncate with an ellipsis (the first name goes first, then the surname is truncated). Layouts must hold strings 30% longer than English (section 10). |
| One-set match | The score panel shows one set column; nothing else changes. |
| Golden point off | Versus rules line reads "ADVANTAGE" instead of "GOLDEN POINT". |
| No points off the glass | The glass rows show 0 with an empty bar; the footer reads "0% of points had a shot off the glass". |
| Match quit from Pause | No stats screen; straight to the main menu (the dialog said the match won't be saved). |
| Match slow to load | Versus card stays with the loading indicator (7.4). After 20 s, a dialog "The match didn't start." with BACK TO MENU. |
| Controller disconnected in a menu | Toast "Controller disconnected."; keyboard and mouse keep working. In a match, Pause opens (7.5). |
| Input during a transition | Ignored for the 120 ms entry lock (section 3), never queued, so nothing is applied twice. |

---

## 8. Copy deck

Every user-facing string, with its `LOCTEXT` key (namespace `PadelMenu`, except where noted). Sentence case for descriptions and dialogs; upper case for titles, tiles and buttons is applied by the style, not typed into the string, so translations can choose.

| Key | Text |
|---|---|
| `Title.Logo` | Padel Street |
| `Title.Tagline` | 2v2 padel |
| `Title.PressAnyButton` | Press any button |
| `Title.PressAnyKey` | Press any key |
| `Title.Build` | Vertical slice · build {0} |
| `Main.QuickMatch` | Quick Match |
| `Main.QuickMatch.Desc` | You and an AI partner against two AI players. |
| `Main.Practice` | Practice |
| `Main.Practice.Desc` | Ball machine drills for every shot. |
| `Main.Settings` | Settings |
| `Main.Settings.Desc` | Audio and accessibility. |
| `Main.Controls` | Controls |
| `Main.Controls.Desc` | See every button. |
| `Main.Career` | Career |
| `Main.TeamBuilder` | Team Builder |
| `Main.Locked.Desc` | Coming in a later update. |
| `Main.Locked.Toast` | {0} is coming in a later update. |
| `Main.Profile.Default` | Player 1 |
| `Badge.ComingSoon` | Coming soon |
| `Badge.You` | You |
| `Badge.Partner` | Partner |
| `Setup.Title` | Quick Match |
| `Setup.YourPair` | Your pair · {0} |
| `Setup.Opponents` | Opponents · {0} |
| `Setup.Rules` | Match rules |
| `Setup.Difficulty` | Difficulty |
| `Setup.Difficulty.Easy` / `.Medium` / `.Hard` | Easy / Medium / Hard |
| `Setup.Difficulty.Easy.Desc` | The AI reads your shots late and plays it safe. |
| `Setup.Difficulty.Medium.Desc` | A fair match. The AI plays smart padel and makes some mistakes. |
| `Setup.Difficulty.Hard.Desc` | The AI reads the ball early, attacks the net and rarely misses. |
| `Setup.Format` | Format |
| `Setup.Format.OneSet` | 1 set |
| `Setup.Format.BestOf3` | Best of 3 |
| `Setup.Format.OneSet.Desc` | First to 6 games, by two clear. Tie-break at 6–6. |
| `Setup.Format.BestOf3.Desc` | First to win 2 sets. Each set is first to 6 games, by two clear, with a tie-break at 6–6. |
| `Setup.GoldenPoint` | Golden point |
| `Setup.GoldenPoint.On.Desc` | At deuce, one deciding point. The receiving pair picks who receives. |
| `Setup.GoldenPoint.Off.Desc` | Advantage scoring: win by two points from deuce. |
| `Setup.Start` | Start match |
| `Card.Overall` | Overall |
| `Card.Attack` / `.Defence` / `.Net` / `.Movement` | Attack / Defence / Net / Movement |
| `Card.Style.*` | Wall play / Net play / Power play / Touch play / All-round (from the roster's style tag) |
| `Card.Side.Left` / `.Right` | Left side / Right side |
| `Card.Hand.Left` / `.Right` | Left-handed / Right-handed |
| `Versus.Vs` | vs |
| `Versus.Rules` | {0} · {1} · {2} |
| `Pause.Title` | Paused |
| `Pause.Resume` | Resume |
| `Pause.Restart` | Restart match |
| `Pause.Controls` | Controls |
| `Pause.Settings` | Settings |
| `Pause.Quit` | Quit to main menu |
| `Pause.Quit.Desc` | The match will not be saved. |
| `Pause.ControllerLost` | Controller disconnected. Reconnect to continue. |
| `Controls.Title` | Controls |
| `Controls.Tab.Gamepad` / `.Keyboard` | Gamepad / Keyboard |
| `Controls.Footer` | Hold a shot button to charge it. Release to swing. Aim with the left stick when you release. |
| `Controls.Later` | Later |
| `Controls.Action.*` | One per GDD 7 row, as in the 7.6 table. |
| `Settings.Title` | Settings |
| `Settings.Tab.*` | Gameplay / Display / Audio / Accessibility |
| `Settings.*` | Labels and descriptions as in the 7.7 tables. |
| `Stats.Win` | {0} win |
| `Stats.Title` | Match stats |
| `Stats.Tab.*` | Summary / Shots / Points |
| `Stats.Row.PointsWon` | Points won |
| `Stats.Row.Winners` | Winners |
| `Stats.Row.Errors` | Errors |
| `Stats.Row.Smashes` | Smashes |
| `Stats.Row.GlassPoints` | Points won off the glass |
| `Stats.Row.DoubleFaults` | Double faults |
| `Stats.Footer` | Average rally {0} shots · {1}% of points had a shot off the glass |
| `Stats.Rematch` | Rematch |
| `Stats.MainMenu` | Main menu |
| `Practice.Title` | Practice |
| `Practice.Body` | The ball machine feeds you balls. Play any shot, including off the glass. |
| `Practice.Start` | Start practice |
| `Practice.Quit` | Quit practice |
| `Hint.Select` / `.Back` / `.Quit` / `.Resume` / `.Skip` / `.ChangeTab` / `.Swap` / `.Reset` / `.Change` / `.StartMatch` / `.Start` / `.MainMenu` | Select / Back / Quit / Resume / Skip / Change tab / Swap player / Reset / Change / Start match / Start / Main menu |
| `Dlg.*` | As in the 7.10 table. |

Copy rules: no exclamation marks; numbers as digits; "AI" not "CPU"; GDD shot names (víbora with its accent, chiquita, bandeja). Team and player names come from the roster (`Padel::Ui` roster, later `Data/Players.json`), never typed into copy.

---

## 9. Motion

Motion is subtle and quick: it confirms input and shows where things came from. It never delays input beyond the 120 ms entry lock.

| Token | Duration | Easing | Use |
|---|---|---|---|
| `Motion.Focus` | 120 ms | ease-out cubic | Focus ring in, scale 1.0 → 1.03, list row slide. Focus out is 80 ms. |
| `Motion.Press` | 80 ms | ease-out quad | Scale down on press. |
| `Motion.Value` | 140 ms | ease-out cubic | Selector value slide, toggle knob, segment fill. |
| `Motion.ScreenIn` | 240 ms | ease-out quint | New screen: slide 48 px from the side it comes from + fade. |
| `Motion.ScreenOut` | 200 ms | ease-in quad | Old screen: slide 32 px + fade. Overlaps ScreenIn by 120 ms. |
| `Motion.Stagger` | 40 ms | — | Delay between items entering (tiles, stat rows). Max 6 staggered items; the rest enter together. |
| `Motion.Dialog` | 160 ms in, 100 ms out | ease-out cubic | Dialog fade + scale 0.96 → 1.0. |
| `Motion.Pulse` | 1.6 s loop | sine | "Press any button". |
| `Motion.PushIn` | 6 s | linear | Focused tile image 1.0 → 1.04. |

**Direction:** going deeper (Main → Setup) slides in from the right; going back slides in from the left. Pause and dialogs fade and scale; they don't slide.

**Reduce motion:** every slide becomes a 160 ms cross-fade, scales and push-ins are off, the pulse is steady, and the backdrop camera is still.

**Sounds** (via `UPadelMatchAudio`'s menu cues; the hooks are new, W10): focus move (soft tick), confirm (racket pop, low level), back (a softer pop), edge (muted thud, like the ball on the mesh), toggle on/off (two pitches), tab change (swish), dialog open (glass ring, very quiet). All are original recordings or synthesised, registered in `SourceArt/manifest.csv`.

---

## 10. Accessibility

- **Focus is never colour alone:** ring shape + scale or slide + raised fill (4.3).
- **Text minimum 26 px at 1080p**, scalable to 1.3× (4.2). Every screen must be laid out and screenshot-tested at Largest (11.5).
- **Contrast** as in 4.1.
- **Team colours** have a high-contrast set (7.7) and never carry meaning alone: team names are always written next to team colour.
- **No timed choices in menus.** The only timers are input locks (Stats, 7.8), which only ever stop a press, never make a choice.
- **Reduce motion** (slice) and **Reduce transparency** (if time) (7.7).
- **Hold to confirm** (post-slice): an alternative to dialogs, where destructive buttons need A held 0.8 s.
- **Controller disconnect** pauses the match (7.5) and shows a toast in menus.
- **Every action reachable with a keyboard alone and a mouse alone**, as well as a gamepad.
- **Localisation-ready:** every string is a `LOCTEXT` (section 8), case is applied by style not typed, and every layout holds strings 30% longer than English (7.12).
- **Subtitles:** not needed for the slice (no commentary or speech, GDD 12).

---

## 11. Implementation notes

### 11.1 Classes

Everything stays in `Source/Padel/{Public,Private}/UI/` (Agent 7's area), built in C++ in `BuildTree()` (decision 0003). Proposed split, each file pair under ~500 lines (CLAUDE.md):

| Class | Role |
|---|---|
| `APadelHUD` | Keeps the screen state machine and match wiring; screen building moves out into one widget per screen. `EPadelScreen` grows (section 3). |
| `UPadelScreenWidget` (abstract) | Base for every screen: header, content area, hint bar, focus map, focus memory, entry/exit animation, input lock. |
| `UPadelTitleScreen`, `UPadelMainMenuScreen`, `UPadelSetupScreen`, `UPadelVersusScreen`, `UPadelPauseScreen`, `UPadelControlsScreen`, `UPadelSettingsScreen`, `UPadelStatsScreen`, `UPadelPracticeIntroScreen` | One per screen in section 7. |
| Components | `UPadelHintBar`, `UPadelPrimaryButton`, `UPadelSecondaryButton`, `UPadelListButton`, `UPadelMenuTile`, `UPadelBadge`, `UPadelPlayerCard`, `UPadelToggle`, `UPadelSelector`, `UPadelSegmented`, `UPadelSlider`, `UPadelTabBar`, `UPadelDialog`, `UPadelToast`, `UPadelScreenHeader` (section 6). |
| `APadelMenuBackdrop` | The menu camera paths (6.15). In `Presentation/` (camera area), so a separate task for that area's owner. |
| `UPadelSettings` | `UGameUserSettings` subclass for 7.7, applied on change. |
| `UPadelMenuWidget` | Retired once every screen has moved; tests move to the new screens. |
| `PadelUiStyle.h` | Kept as the one place widgets get style from, but its constants become accessors on the loaded `FPadelUiTheme` (11.2). |

**Plain UMG, as decision 0003 says.** Nothing in this spec needs CommonUI. The screen stack, focus memory, hint bar and device switching are implemented in `UPadelScreenWidget` and `UPadelHintBar` with plain UMG and `UInputDeviceSubsystem` (already used by `APadelHUD::ButtonLabel`). All animation is driven from C++ (`NativeTick` with the motion tokens in section 9), not Widget Blueprint timelines, so it stays reviewable text. Moving to CommonUI later (`UCommonActivatableWidget` stacks, `UCommonActionWidget` glyphs) would mean changing decision 0003 in its own PR, with Agent 7 as owner (13 Q9); the screen and component split here is designed so that swap only touches the two base classes.

### 11.2 Theme data

`Data/UiTheme.json` holds every token in section 4 and 9, plus `PlayerCardStats` (6.6). Read at startup into `FPadelUiTheme` (a `USTRUCT` row type in `Data/`, per CLAUDE.md) with defaults if the file is missing, as `Data/Camera.json` is read today (decision 0003). Its schema is a shared contract, so it lands in its own PR with the title saying so.

### 11.3 Saved state

`UPadelSettings` saves the 7.7 values and the last match setup (difficulty, format, golden point, player) to `GameUserSettings.ini`. Nothing else is saved in the slice.

### 11.4 Work items

In the order they unblock each other. "Area" is the folder that owns it.

| # | Work | Area | Depends on |
|---|---|---|---|
| W1 | `Data/UiTheme.json` + `FPadelUiTheme`, replacing `PadelUiStyle.h` constants (shared contract PR) | Data, UI | — |
| W2 | Barlow fonts in `SourceArt/Fonts/` + manifest rows + import | Asset pipeline | — |
| W3 | Screen base class, hint bar, focus maps, focus memory, input lock, Start/Back fix (G13–G15) | UI | W1 |
| W4 | Components (section 6) with native-model tests for their state logic | UI | W1, W3 |
| W5 | "Match ready" signal for Versus (7.4) | Gameplay (match owner) | — |
| W6 | Overall rating weights in `Data/Players.json` and `OverallRating()` (13 Q1) | Data, UI model | GDD 6.2 decision |
| W7 | Screens 7.1–7.8 | UI | W3, W4 |
| W8 | Stats model: points won with an off-glass shot per team, smash count row, tie-break points per set | UI model | — |
| W9 | Practice entry and drill names exposed by `APadelPracticeDirector` | Gameplay | — |
| W10 | Menu sound cues | Presentation | W7 |
| W11 | Menu backdrop camera paths | Presentation | Court level |
| W12 | Tile images and player portraits rendered by the asset pipeline | Asset pipeline | Characters |
| W13 | `UPadelSettings` and Settings screen | UI | W3, W4, W15 |
| W14 | A play style field in `Data/Players.json`, so the card stops reading the UI roster's style tag | Data (Agent 3) | — |
| W15 | Sound classes and a sound mix for the volume sliders | Presentation | — |

### 11.5 Tests

- **Native (Tests/Native, CI on every PR):** the engine-free parts. Selector stepping and no-wrap/wrap rules, toggle and segmented logic, focus map tables (every focusable element has a target or an explicit "none" for all four directions, and every element is reachable from the default focus), card stats from `Players.json`, overall rating, the stats-model additions (W8), the copy-deck keys exist.
- **Unreal Automation (`Padel.UI.*`, on the Mac):**
  - `Padel.UI.Flow.QuickMatchThreePresses`: any button, A, A from the Title reaches the Versus screen.
  - `Padel.UI.Flow.GamepadOnlyFullMatch`: GDD 12 item 1, the demo match from Title to Stats with simulated pad input only.
  - `Padel.UI.Focus.<Screen>`: every screen opens with its default focus, and B returns focus to the opener.
  - `Padel.UI.Pause.Blocked`: Start does nothing during the golden point prompt, Versus and the end card.
  - `Padel.UI.Dialog.SafeDefault`: every dialog opens on its safe option and B picks it.
  - `Padel.UI.Stats.StrayConfirm`: an A held from the last point, and an A pressed during the entry animation, neither start a rematch; a fresh A after the lock does.
  - `Padel.UI.Setup.SwapAnywhere`: X swaps the player from every focusable element on Setup.
  - `Padel.UI.Settings.Persist`: a changed setting survives a reload.
  - `Padel.UI.Layout.LargestText`: each screen at Text size Largest has no clipped text (each `UTextBlock`'s desired size fits its slot).
- **Screenshots:** a capture of each screen at 1920×1080 and 2560×1600 (16:10 Mac) at default and Largest text, attached to the implementing PR.

---

## 12. Slice scope: must, if time, post-slice

The GDD 12 "In" list names the main menu, setup and stats, and a "simple" menu means the slice builds the must column first and stops there if time runs short.

| | Slice: must | Slice: if time | Post-slice |
|---|---|---|---|
| **Screens** | Title (7.1), Main menu (7.2), Quick Match setup (7.3), Pause (7.5), Controls, read-only (7.6), Settings, cut down (7.7), Stats (7.8), confirm dialogs (7.10), Practice entry and pause (7.9; needs Caio's yes, 13 Q10) | Versus card (7.4; without it Start goes straight to the match, as today) | Career, Team Builder, profiles, online, local multiplayer setup, team and partner choice beyond Lagoon, replays, tutorial |
| **Settings** | Audio volumes, Text size, Reduce motion | Timing cue, Partner command prompts, Reduce transparency, high-contrast team colours | Display tab, Brightness, Vibration, Stick dead zone, Hold to confirm, rebinding |
| **Components** | Hint bar, primary, secondary and list buttons, tile, badge, player card (no Details), toggle, selector, segmented control, slider, tabs, dialog, toast, screen header | — | Profile chip, card Details overlay |
| **Presentation** | A fixed menu camera shot on the court (one per screen family), the single blur layer, motion tokens | Backdrop camera paths (6.15), menu sounds (W10) | Idle players on the menu court, menu music |
| **Art** | Locked tiles as flat glass plus a line icon; portraits as team-colour silhouettes; tiles over a still of the court | Rendered tile images and player portraits (W12) | — |
| **Shown but locked** | Career and Team Builder tiles, the drop shot row on Controls | — | — |

## 13. Open decisions for Caio

Each has a default, so building isn't blocked.

1. **Overall rating weights (GDD 6.2).** The GDD names the heavier attributes per side but no numbers. Default: show the plain average until Agent 3 adds weights to `Data/Players.json`, in a GDD-change PR.
2. **Locked tiles in the playtest build.** Show Career and Team Builder as "Coming soon"? Default: yes, as Caio's brief asked.
3. **Switch pad confirm button.** Confirm is the bottom face button on every pad (so "B" on a Switch pad). Default: keep it by position, as the code does today.
4. **HUD accent.** Move the HUD's yellow callouts to lime too? Default: no, lime is too close to the ball when it moves; keep yellow in the HUD until the HUD pass, or pick a white-on-charcoal callout instead.
5. **Choosing teams.** Let the human play for Ember, or choose any two of the four as a pair? Default: no for the slice (GDD 6.4 fixes the pairs).
6. **Match length copy.** Show "about 15 / 35 minutes" on the format descriptions once the soak test measures it? Default: yes, once measured.
7. **Player card Details.** Add the Y "Details" overlay with all 23 attributes (6.6) after the slice? Default: post-slice; it makes all four cards focusable when it lands.
8. **Stick dead zone setting.** GDD 7 says dead-zone settings are post-slice. Default: leave the row out unless Agent 1 exposes it cheaply.
9. **CommonUI.** Move from plain UMG to CommonUI (11.1)? Default: not for the slice; decision 0003 stands.
10. **Practice in the slice menu.** Keep the Practice tile live (GDD 12 item 3 can't be met without the ball machine), or lock it and keep practice a dev-only map? Default: live. GDD 12's "In" list doesn't name practice, so a yes needs a one-line GDD 12 edit in its own PR (CLAUDE.md: GDD changes go in their own PR).

---

## Appendix A. Prompt for Claude Design

Paste this to get mock-ups. They come back as **reference images only**: the build stays C++ widgets (decision 0003), and the values in this spec win over anything in a mock-up.

> Design the menu screens for an original 2v2 padel video game, working title "Padel Street" (a working title only). Mock up every screen below at 1920×1080, for a gamepad on a TV seen from 2.5 m.
>
> **Hard rules.** Everything is original: no real brands, leagues, tours, players, logos, and no layouts, tile grids, fonts or screen names copied from any existing sports game. Aim for the polish of a modern TV sports broadcast, but simpler. Only Quick Match, Practice, Settings and Controls work; Career and Team Builder are shown as locked "Coming soon" tiles that clearly look locked, not broken: a flat glass panel with a simple line icon and a padlock, no photo, with the label still easy to read.
>
> **Style.** Deep navy `#070B14` and charcoal `#151922`, glassy translucent panels (`#111A2B` at 72%, a 1 px white edge at 10% and a brighter 1 px top edge, over one soft full-screen blur of the background, not a blur per panel) as a nod to the court's glass walls, and one accent, electric lime `#C8F03C`, used only for focus, the main button and selected marks. Text warm white `#F4F1E8`. Team Lagoon teal `#2EC4B6`, Team Ember orange `#FF7A3D`. Font: Barlow Condensed (titles, labels, numbers with tabular figures) and Barlow (body). No text smaller than 26 px. 12-column grid inside a 96 px side and 64 px top/bottom safe area. Corners 12 px on panels, 8 px on buttons. The live court (blue turf, glass, dark frame, arena lights) is always the dimmed background.
>
> **Focus** is a 3 px lime ring with a soft glow plus a 3% scale-up (list rows: a lime bar on the left and an 8 px slide), never colour alone. Every screen has a button hint bar bottom right ("A Select  B Back") with neutral, original button glyphs.
>
> **Screens.**
> 1. **Title:** "PADEL STREET" wordmark, "2V2 PADEL", "Press any button" pulsing, over a slow shot of the court.
> 2. **Main menu:** a large Quick Match tile (8 columns, players mid-rally at the net) with Practice and Settings tiles stacked on its right, and a bottom row of Career (locked), Team Builder (locked) and Controls.
> 3. **Quick Match setup:** left, "YOUR PAIR · LAGOON" with two large player cards (our own broadcast "player graphic", not a sports-game trading card: a name strap in the team colour across the top with first name over surname and a YOU or PARTNER badge, a waist-up portrait standing on the strap's lower edge and set to the left, a profile line in words on its right such as "Left side, Right-handed, 1.86 m, Power play", four stat rows with full-word labels Attack, Defence, Net, Movement, each a bar and a number, and at the bottom an "Overall" meter in the team colour with its number; no big rating or position code in a corner); right top, "OPPONENTS · EMBER" with two small cards (name strap and Overall meter only); a "SWAP PLAYER" hint on X; right bottom, "MATCH RULES" with Difficulty ‹ MEDIUM ›, Format [1 SET | BEST OF 3], Golden point ON/OFF toggle, and a lime START MATCH button (focused). Players: Mikko Ventara 77 (left side, power), Selva Arunde 78 (right side, wall), Ilan Corvisa 78 (right side, all-round), Ruben Halvaro 77 (left side, touch).
> 4. **Versus card:** LAGOON vs EMBER, player names, rules line "MEDIUM · BEST OF 3 · GOLDEN POINT".
> 5. **Pause:** over the frozen match, "PAUSED", score line top right, a list: Resume, Restart match, Controls, Settings, Quit to main menu, with a context panel on the right.
> 6. **Controls:** an original, generic two-stick gamepad line drawing with labels: Move and aim, Sprint, Drive/volley/serve, Lob, Overhead, Attack modifier, Touch modifier, four partner commands on the D-pad, Pause; a Gamepad/Keyboard tab bar.
> 7. **Settings:** tabs Audio and Accessibility; volume sliders (Master, Ball and court, Crowd, Menu sounds), Text size selector and Reduce motion toggle on the left, and a description panel on the right.
> 8. **Post-match stats:** "LAGOON WIN" in teal, a score panel by set (6–4, 3–6, 7–5), tabs Summary/Shots/Points, rows with centre-out comparison bars (Points won, Winners, Errors, Smashes, Points won off the glass, Double faults), a footer "Average rally 8.4 shots", and REMATCH (lime) and MAIN MENU buttons.
> 9. **Dialogs:** "Quit to main menu?" with QUIT MATCH (red) and KEEP PLAYING (focused).
>
> **Deliver** a style sheet (colours, type scale, spacing, the components: tile in default/focused/locked, player card large and small, selector, toggle, segmented control, slider, list button, primary button, tab bar, hint bar, dialog, toast) with every state (default, focused, pressed, disabled, locked, selected), then each screen mocked up, plus the main menu with the Career tile focused to show the locked state.
