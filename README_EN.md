# EFF-HotFix: Endfield Resource Replacement Framework (Reload Hotfix)

[中文](README.md)

> [!IMPORTANT]
> Built upon upstream [ssice-a/EFF](https://github.com/ssice-a/EFF) `v2.0.12`, this fork fixes severe mesh distortion and reload failures when toggling or hot-reloading Mods in-game via `F10` (or custom `reload` hotkeys).

## HotFix Changes (v2.0.12-hotfix.2)

- **Fix ~38s Scene-Loading Freeze**: Fixed standalone `.asset` Mesh targets (e.g. character LOD0 `.asset` assets) missing `EiemDemandLogicalAssetLookup` / `DemandLoadingProbe::Current()` stem matching and falsely accumulating in `state->unresolved`, which previously triggered repeated `SharedInputs` scans on every asset load; also replaced hot-path `IsBadReadPtr` / `IsBadWritePtr` calls with zero-exception canonical pointer checks and switched logging to 64KB buffered periodic flushes.
- **Eliminate ~2s Team-Switch Character Stall**: `EiemNativeRandomAccess` now keeps immutable in-RAM snapshots for frozen author files (`<= 32 MB`) instead of writing synchronous `%TEMP%` snapshot files, pre-filters `pathId`/`classId` via `AssetsFiltered` before LZ4 block decompression, and caches `m_Container` indices; `EiemNativeSpliceView` coalesces adjacent uncompressed archive blocks to eliminate ~1,500 tiny `4B`/`16B` splice chunks.
- **Fix `0 → 1` and `1 → 0 → 1` Hot-Reload Mesh Distortion**: Extended `DemandGenerationReload::Affected()` to match target vanilla source Mesh names (stripping `(Instance)` / `(Clone)` suffixes), ensuring already-instantiated world characters, UI preview models, and cached Prefabs properly rebuild and rebind skeletons when enabling a Mod after launching without Mods (`0 → 1`) or re-enabling after disabling (`1 → 0 → 1`).
- **Fix Native `AssetBundle` (`CAB-*`) Collisions on Reload**: Drains idle reverse-dependent proxies in `BundleWaitUnloadState` and safely retires loaded native source `BundleProxy` / `AssetBundle` containers before submitting `versionedRoot` bundles, while unloading synthetic `CAB` identities on `1 → 0` passthrough loads.
- **Compiler Portability**: Added dual MSVC and Clang / Zig C++ toolchain compatibility.

---

AnimeStudio exports source assets and templates. Blender creates and compiles each independent Mod. EFF applies the compiled rules before the game creates resources and uses the game's model lifecycle for reloads.

Paired releases: **EFF v2.0.12-hotfix.2, EFF Blender v0.50.7, EFF Mod Manager v0.2.3, AnimeStudio v1.9.5**. The DLL accepts Format 3 only; Mod Manager converts legacy packages offline.

See the illustrated quick tutorial for installation, extraction, Blender editing and export: [English](Mod%20Tutorial_EN.md) · [中文](Mod%20Tutorial.md).

## Install

Download from [EFF-HotFix Releases](https://github.com/BakaMai/EFF-HotFix/releases/latest) after quitting the game:

- **Existing EFF Upgrade / EFMI Coexistence (Recommended)**: Download the standalone **`eff.dll`** and overwrite `plugin/eff.dll` beside `Endfield.exe`. This preserves your `plugin/eff.ini` hotkeys and will not overwrite EFMI or [EFF-EFMI-Compatibility-Plugin](https://github.com/BakaMai/EFF-EFMI-Compatibility-Plugin) proxy files.
- **Fresh Standalone Install**: Download **`EFF_v2.0.12-hotfix.2_dll.zip`** and extract beside `Endfield.exe` (proxy forwarders currently support Windows installed at `C:\Windows`).

```text
d3dcompiler_47.dll
vulkan-1.dll
plugin/
  eff.dll
  eff.ini
  package-protocol.json
  mods/
    YourMod/
      mod.ini
      meshes/
      materials/
      textures/
```

The production DLL and proxies statically link their own C/C++ runtime implementation and do not depend on the game's older MSVCP/VCRUNTIME files. Do not replace the game's runtime files. The DLL distribution has no compiler, source tools or AnimeStudio libraries. Previous `plugin/tools/eff_resource_pack.exe` and `plugin/tools/static-sources/` are unused and can be removed. Move complete Mod folders directly into `plugin/mods/`. Migrate legacy Mods with Mod Manager or re-export with the paired Blender add-on. Format 3 ships independent .effres and DDS resources without compiled.bin or source-inputs.bin. Relative game source paths make the output portable. After game updates, players can use the Mod repair feature in EFF Mod Manager; it matches current game resources and backs up the original package without Blender or manual extraction. Authors can instead refresh extracted sources in AnimeStudio and export their original projects again.

## Features and controls

- Independent character Mods; characters without a Mod retain original assets.
- Skinned Mesh and static MeshFilter/MeshRenderer replacement, materials, textures and visibility. Static Renderers do not support runtime shape controls.
- One source Renderer can expand into multiple author parts. Unspecified targets retain original content. Materials and textures may use game templates or author-created data.
- **Ins** opens independent keyboard checkboxes for each Mod, all enabled by default. Multiple Mods can respond to the same key. The dropdown selects the Mod whose buttons and sliders are shown. Key switches, shape controls and saved player values remain supported.
- **F10** reads the full current Mod set, handles additions/removals/changes and the empty set, and reloads supported world/display owners with animation, weapon and audio rebinding. Failed preparation or submission keeps the current generation.
- Shape Keys clips have independent playback groups and play/stop shortcuts. Ins provides pause and persistent live speed controls without resetting playback phase.
- Effects can ship as standalone Mods or attach to a character Mesh bone. They follow the model lifecycle, can be toggled by key, reload with all Mod resources through F10, and cover all LODs.
- Animated material parameters and effect resources use the same Mod export and reload pipeline; Mods are discovered recursively below `plugin/mods`.

UI and world instances of a Mod share control values. UI reload uses the game's
loader and page item binding, retaining the displayed model until candidate
verification, page binding and control publication complete. Instances without
a verified page owner reject reload before mutation.

Set `key_switch_enabled=0` or `1` under `[Mod]` in each `mod.ini`, then press F10 to apply manual edits. An omitted flag defaults to enabled. The Ins checkboxes apply immediately and save this same field; disabling keys preserves the Mod's resources and current values.

DLL updates require a game restart. Wait for active model loads to finish before requesting reload. Global shortcuts and camera fade behavior are configured in `plugin/eff.ini`.

## Author workflow

1. Export an EFF source package using [AnimeStudio](https://github.com/ssice-a/AnimeStudio). Keep source templates, manifest and author index.
2. Import its `mod.ini` with [EFF Blender](https://github.com/ssice-a/EFF-blender), edit and export selected Meshes. A complete export validates all declarations and only replaces files whose contents changed.
3. Blender obtains shared authoring tools from EFF Releases, resolves all consumers of the declared source Mesh offline and compiles this one staged Mod. Verified tools are cached under the user-selected resource root before publishing.
4. EFF reads Format 3 resources on demand, combines edits on shared inputs and submits native inputs before deserialization. It does not compile geometry/materials/textures, query the global asset index or scan scene Renderers.

The four-endpoint ownership, user-selected resource root, AnimeStudio index location and build/release flow are described in [Shared Tools and Indexes](docs/shared-tools-and-indexes.md). Blender 5.2 users select the AnimeStudio unpack-index location in the add-on UI after installation; they do not select an extraction output directory.

Exports under the game's `plugin/mods/` locate the installation automatically; other output directories require `EFF_RELOAD_GAME`. Players need neither author tools nor source snapshots. `mod.ini` remains the author rule entry; Format 3 stores independent resource files.

Complete original bundles are not saved in the Mod. Format 3 stores authored resources and declarations; native inputs are prepared when the game requests them. Package size is not an estimate of Unity memory usage. See the [loading flow](docs/runtime-resource-discovery.md) and [output contract](docs/offline-resource-package-format.md).

## Validation limits

The current update has regression coverage for BLC v5, portable full indexes, transactional extracted-source refresh, an existing Blender project and old-Mod repair. Update adaptation runs outside the game. The DLL performs no background discovery, compilation or Mod writes. Broad in-game visual coverage, active-animation performance and substantial skeleton changes still require separate validation.

Regression checks cover shared-PFB composition, removals, stream relocation, static Meshes, sequential world/display reloads, animation consumers and rollback. The latest in-game two-character F10 took about 107 ms from request to commit, with idle attachments visually confirmed. This measured an already prepared candidate, not every scene's latency. Independent crowd NPC/scene-object refresh, complete generation retirement and 20–30 Mod pressure remain unverified. Exporting all LOD targets does not generate reduced geometry. New BoneCloth chains and sphere/capsule colliders are connected to offline compilation. Animation blending, complete physics teardown and F10 still require in-game acceptance. This release build passed automated regressions; new in-game acceptance has not been performed for these binaries.

## Credits and license

AnimeStudio and its contributors provide the asset browsing, extraction, and export foundation. Third-party licenses are listed in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES). EFF is released under AGPL-3.0. The project contains no game assets; game assets remain the property of Hypergryph.

## User Agreement & Disclaimer

<details>
<summary>Please read this agreement before downloading, installing, or using EFF. By using EFF, you acknowledge that you have read, understood, and agreed to the following terms.</summary>

### 1. Open Source License & End User Rights

- EFF is open-sourced under the **AGPL-3.0** license. You may use, modify, and distribute the source code in compliance with that license.
- End users may use and distribute the unmodified software itself.

### 2. Anti-Fraud Statement

- Do not sell the EFF software itself on an online marketplace without providing the repository address and necessary support.
- EFF is freely available on GitHub. If you paid for the software itself, it can still be obtained from GitHub for free.

### 3. Content Compliance & Conduct

- EFF contains no game art assets. Official animations, scenes, models, and other assets in *Arknights: Endfield* belong to Hypergryph and are not covered by AGPL-3.0.
- Do not use EFF or official game assets to create, play, or distribute content that violates laws, the game's rules, or community standards.

### 4. Risk & Disclaimer

- EFF is provided for learning, technical research, and discussion. Use of this tool may violate the game's terms of service and may carry risks including account suspension or data loss.
- Users bear all direct and indirect risks and losses arising from use of EFF. The project assumes no legal or financial liability. Test accounts are recommended.

</details>
