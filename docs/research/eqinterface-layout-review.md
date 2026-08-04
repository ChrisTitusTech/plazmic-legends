# EQInterface layout review

Date: 2026-08-03

## Purpose

This review identifies layout principles that can improve the private
2560x1440 Plazmic UI without importing incompatible EverQuest UI files or
assets. EverQuest Legends uses its own installed `default_modern` schema, so
all implemented output remains derived from the user's current Legends layout.

## References

- [AYA SoR](https://www.eqinterface.com/downloads/fileinfo.php?id=5518)
  concentrates mouse-driven commands near the middle-bottom and keeps the
  central viewport open.
- [ZlizUI Minimalist](https://www.eqinterface.com/downloads/fileinfo.php?id=6417)
  uses compact modules that visually snap into a continuous lower strip.
- [4k/1440p Upscale Kunark/Velious UI](https://www.eqinterface.com/downloads/fileinfo.php?id=6859)
  treats 1440p as a deliberate scaling target instead of spreading smaller
  elements across the larger canvas.
- [DuxaZeal1440p](https://www.eqinterface.com/downloads/fileinfo.php?id=6994)
  offers 1440p-sized windows and variants for different information density.
- [Ang's UI](https://www.eqinterface.com/downloads/fileinfo.php?id=4585)
  demonstrates why current-client schema compatibility must be validated after
  UI updates rather than assumed from visual similarity.

These pages are design and compatibility references only. Their XML, layout
files, images, and other assets are not copied into Plazmic Legends.

## Functional baseline

The private live screenshot at `captures/eq-ui-functional-20260803.png` records
the working 2560x1440 baseline. It remains ignored because it contains private
runtime information. Its SHA-256 is
`d6b60df7c2992cefee01013790641d80d350afff83b1b6c141108ee29aa03bd7`.

The baseline is functional but visually fragmented:

- the group controls and secondary chat are isolated at the top-left;
- spell gems and extended targets form an unaligned left rail;
- player, target, stance, and hotbar windows use inconsistent center offsets;
- the primary and combat chats do not share a common vertical rhythm; and
- the map is usable but undersized relative to the 2560x1440 canvas.

## Adopted layout rules

The generated `UI_plazmic_1440p.ini`:

- preserves every source section, unknown key, and Legends-only value;
- changes only an allowlisted set of standard geometry keys;
- aligns primary and combat chat windows along the lower edge;
- places secondary chats directly above their corresponding lower chat;
- centers the two active hotbars and aligns player, target, and stance data;
- groups spells, group state, and extended targets into a left combat rail;
- enlarges and aligns the active map at the top-right; and
- retains all visibility, filter, channel, opacity, and character-specific
  values from the source.

The layout stays unlocked for live adjustment. The private installer presents
it as a source choice and still asks which current layout INI receives it. For
live testing, `/copylayout` loads that written target, then
`/loadskin plazmic-ui 1` reloads the skin while retaining the copied geometry.

## Compatibility boundary

- The newest installed Legends `UI_*.ini` is the only transformation source.
- Missing required Legends sections fail closed instead of producing a partial
  layout.
- EQInterface layouts from Live, TAKP, Project Quarm, P99, or other clients are
  not accepted as source material.
- No EQInterface asset, XML implementation, layout file, or private screenshot
  enters the repository, package, release, or pull request.
