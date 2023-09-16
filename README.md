# GPTP-For-VS2008.
--------
The General Plugin Template Project of Starcraft adapted to Visual Studio 2008

Branches
--------

I publish in two rolling branches:

The **[release branch](https://github.com/KYSXD/GPTP-For-VS2008/tree/release)** is extensively tested. I work hard to make releases stable and reliable, and aim to publish new releases every few months.
This branch tracks all the changes i made for *Starcraft V*.

The **[update branch](https://github.com/KYSXD/GPTP-For-VS2008/tree/update)** tracks live changes by [BoomerangAide](https://github.com/BoomerangAide/GPTP-For-VS2008). 
This is the cutting edge and may be buggy.

> **Important**: Note from [BoomerangAide](https://github.com/BoomerangAide)
*-If there are several Update-X branches, the most stable is the one with the lowest value, the less tested (but probably with more experimental features) is the one with the highest value.*

Other short-lived branches may pop-up from time to time as i stabilize new releases or hotfixes.

### Work done:

- Starting workers set to 12.
- Workers will harvest at first run.
- Re-worked collision for workers while harvesting (SCII inspired).
- Window-mode (requires WMODE.dll and WMODE_FIX.dll).
- Changed harvest rates (and could be controlled separately).
- Changed unit-speed behavior and added speed modifier on creep.
- Changed cooldown behavior and added stim variants (this would allow other units to use stim for other purposes).
- Auto harvest on rally point.
- Auto enter-transport on rally point.
- Target lines.
- Idle worker count.
- Different larva amount (3 - Hatchery, 4 - Lair, 5 - Hive).
- Dark/High Templar merge (Twilight archon inspired).
- WarpGate plugin (SCII inspired).
- Increased regen to burrowed units (also, improved regen after upgrade).
- Decrease shield for unpowered buildings.
- Zealot's charge (SCII inspired).
- Different harvest rates for minerals (red, blue and yellow minerals).
- Improved shield regen (SCII inspired).
- Chrono boost (SCII inpired).
- Terran Reactor behavior (SCII inspired).
- Stalker's blink (SCII inspired).
- Adept's psionic transfer (SCII inspired).
- Nydus canal rally point.
- Zerg burrowed movement.
- Smart Casting.
- Starting hangar units (for carrier/reaver).
- Terran/Protoss building placement preview.
- Add worker count for CC, Nexus and Hatchery/Lair/Hive.

**To do:**

- Add rally point for zerg-eggs.
- Add code for burrowed movement.
- Add worker count for Refinery, Assimilator and Extractor.
- Add 12/15 button box (from the 3x3 command box).
- Rework the pathfinding, like SCII pathfinding, based on the [Daedalus Lib](https://www.youtube.com/watch?v=SDH1AZLMZkY).
- Rework the game layer size in order to achieve [this HUD](http://www.moddb.com/members/kysxd/images/hud-20-wip1 ).
- Add code to warp units on creep.
- Add code to decrease zerg buildings hp without creep.

### Tutorials:

_Now you can check the wiki to learn how every plugin was developed!_:

Check now the
[Wiki](https://github.com/KYSXD/GPTP-For-VS2008/wiki)
to find all the available tutorials.

**If you have any request...**

Feel free to send a message to KYSXDreplays@hotmail.com.

Also, you can take a look to my [youtube channel](https://www.youtube.com/user/KYSXD) to look at demos and gameplays.
