# MetaHuman players: making the four

How to make the four slice players in the MetaHuman Creator inside UE 5.8 and get them into the match. Why it works this way is in [`docs/decisions/0006-metahuman-players.md`](../decisions/0006-metahuman-players.md).

Everything here happens in the editor on the Mac. Nothing you make in these steps is committed: `Content/Padel/Characters/MetaHumans/` is git-ignored, so back that folder up yourself.

## Before you start (once)

1. Pull the branch and open `Padel.uproject`. Say **Yes** if it asks to rebuild modules. The MetaHuman Creator plugin is already enabled in the project; if the editor asks to restart for it, restart.
2. The MetaHumans copy the pose of the placeholder skeleton, so it must exist and move first:
   - `SK_PlayerPlaceholderA` from the asset pipeline (`python3 Tools/AssetPipeline/pipeline.py all`).
   - The locomotion clips: in the Output Log (Python), `exec(open("/Users/caiobueno/Developer/padel/Tools/AssetPipeline/unreal/animation_setup.py").read())`. It ends with `[padel-anim] done`. No `ABP_PadelPlayer` is needed (0006, "Moving without ABP_PadelPlayer").
3. If the Creator says **MetaHuman Creator core data is missing**: Epic Games Launcher, **Unreal Engine › Library**, the 5.8 tile's **▾ › Options**, tick **MetaHuman Creator Core Data**, **Apply**.
4. Sign in with your Epic Games account when the Creator asks: auto-rigging and the texture download run on Epic's servers.

## Make one player (repeat for each of the four)

Do Selva first, all the way to step 9, then check it in a match before making the other three.

1. **Create the character.** In the Content Browser, make the folder `Content/Padel/Characters/MetaHumans/Source`. Right-click inside it, **MetaHuman › MetaHuman Character**, and name it exactly as in the table below (`SelvaArunde`). Double-click it to open the Creator.
2. **Face.** In **Blend**, start from 2 or 3 of the Creator's presets and blend between them, then refine in **Sculpt** toward the look in the table. Don't use **From Footage / Mesh / Identity** tools and don't copy a real person: the players must be invented (CLAUDE.md hard rules).
3. **Skin, eyes, teeth** from the table.
4. **Hair and face hair** (Grooms): pick the closest style from the table. Short styles are cheaper.
5. **Body.** Set **Height** to the table's height and the build from the table (athletic for all four).
6. **Wardrobe.** In **Hair & Clothing › Selection**, wear the default garment (shirt and shorts). You don't need to colour it: `metahuman_setup.py` paints the shirt and shorts in the player's team kit from `palette.json` (`Kit` in `Data/PlayerBodies.json`) every time it runs. UE 5.8's built-in wardrobe has no shoes or socks; trainers need a MetaHuman clothing pack added to the project.
7. **Auto-Rig.** Choose **Joints Only** (no blend shapes) and run it. Wait for it to finish.
8. **Textures.** Download the texture source at **2K**.
9. **Assemble.** In **Assembly**, pick the **optimised** pipeline (the one for games, not "Cine") at **Medium** quality. Set the output (root) folder to `/Game/Padel/Characters/MetaHumans` and the name to the same name as step 1. Click **Assemble**. You should get `/Game/Padel/Characters/MetaHumans/SelvaArunde/BP_SelvaArunde`.

## After assembling (each time)

1. In the editor's **Output Log**, switch the input to **Python** and run:
   `exec(open("/Users/caiobueno/Developer/padel/Tools/AssetPipeline/unreal/metahuman_setup.py").read())`
   It builds the retargeter (the first time it needs one assembled MetaHuman, so run it again after Selva), then lists each player as found or still on the placeholder. If it says it found a Blueprint with a different name, put that path in `Data/PlayerBodies.json` (`MetaHumanClass`, ending in `_C`) and tell me, so the PR carries it.
2. Open the retargeter once (`/Game/Padel/Animations/Retarget/RTG_PadelPlaceholder_To_MetaHuman`), pick an idle or swing clip in its preview and check the arms and feet look right. If the arms are raised or the feet slide, tell me what you see before changing anything: the script makes this asset, so the fix goes in the script.
3. Play a match. The log line `Player selva_arunde: showing MetaHuman ...` confirms the swap. Type `stat fps` and `stat unit` in the console: the target is 60 fps with all four players on screen.

## The four players

All four are invented. Ratings and heights are from `Data/Players.json`; sides and roles from GDD 6.4.

| Name (asset name) | Team, side, role | Height, build | Look |
|---|---|---|---|
| **Selva Arunde** (`SelvaArunde`) | A (red), right side, wall defender, right-handed | 1.78 m, lean and wiry, an endurance build | Late twenties. Medium tan skin, dark brown eyes. Short dark curly hair, close-trimmed full beard. Calm, focused face. |
| **Mikko Ventara** (`MikkoVentara`) | A (red), left side, power player, right-handed | 1.86 m, the tallest: broad shoulders, heavier upper body | Early thirties. Fair skin with freckles, grey-blue eyes. Light brown hair in a short buzz cut, clean-shaven, square jaw. |
| **Ilan Corvisa** (`IlanCorvisa`), the star | B (purple), right side, all-rounder and the flashy one, right-handed | 1.81 m, light and springy, the most athletic of the four | Early twenties, the youngest. Warm light-brown skin, dark brown eyes. Dark curly hair worn fairly long on top, short at the sides, no facial hair. A bright, cocky grin. Build him from the Creator's presets; he is invented, not modelled on any real player. |
| **Ruben Halvaro** (`RubenHalvaro`) | B (purple), left side, touch player, left-handed | 1.79 m, slim | Mid thirties, the oldest. Light olive skin, hazel eyes. Longer dark hair swept back with grey at the temples, a sweatband, short stubble. |

Change any look you like: only the asset names have to match `Data/PlayerBodies.json`.

## If it's under 60 fps

In `Data/Animation.json`, in this order, checking `stat fps` after each:

1. `MetaHumanMinLod` from 1 to 2.
2. Re-assemble the players at **Low** quality.
3. `MetaHumanIkLodThreshold` from 1 to 0.
