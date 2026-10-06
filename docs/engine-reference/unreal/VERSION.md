# Unreal Engine — Version Reference

| Field | Value |
|-------|-------|
| **Engine Version** | Unreal Engine 5.8.3 |
| **Installed at pin time** | **5.8.3** — `E:/UE_5.8`. `Engine/Build/Build.version`: MajorVersion 5, MinorVersion 8, PatchVersion 3, Changelist 58210709, BranchName `++UE5+Release-5.8`, IsPromotedBuild 1. Probed 2026-10-06. |
| **Release Date** | 5.8 is released (official release-notes page exists and is current) — exact date NOT SOURCEABLE, see below |
| **Project Pinned** | 2026-10-06 |
| **Last Docs Verified** | 2026-10-06 |
| **LLM Knowledge Cutoff** | May 2025 |

> **Release date sourcing note.** The 5.8 release-notes page carries a `5.7`,
> `5.6` and `5.5` entry in its own version switcher, and the versioned URL used
> below (`…release-notes?application_version=5.8`) serves 5.8 content — so 5.8's
> release is confirmed. No day-level date was found on a page fetched this run,
> so none is written. Prior entries in this file's timeline are approximations
> for the same reason.

## Knowledge Gap Warning

The LLM's training data likely covers Unreal Engine up to ~5.3. Versions 5.4, 5.5,
5.6, 5.7 and **5.8** introduced significant changes that the model does NOT know
about. Always cross-reference this directory before suggesting Unreal API calls.

## Installed-Version Gap Warning

The warning above is one-directional — it covers the **model** knowing less than
this pin. The reverse gap is real and `/setup-engine` §3 creates it deliberately
("pin the newer one and upgrade later"): this reference can sit **ahead of the
installed editor**, and an agent citing it correctly then emits APIs that do not
compile locally. **Check `Installed at pin time` above before trusting a
version-qualified claim** — `NOT DETERMINED` means the gap is unknown, not absent.

*Status for this project (2026-10-06): the gap is **closed**. The pin (5.8.3) and
the installed editor (5.8.3, `E:/UE_5.8`) match exactly, and headless boot was
verified by running it — see `commands.smoke` in `project.yaml`. Note the
`.uproject` records only `"EngineAssociation": "5.8"` because that field is
major.minor by format; that is not a version mismatch.*

## Post-Cutoff Version Timeline

| Version | Release | Risk Level | Key Theme |
|---------|---------|------------|-----------|
| 5.4 | ~Mid 2025 | HIGH | Motion Design tools, animation improvements, PCG enhancements |
| 5.5 | ~Sep 2025 | HIGH | Megalights (millions of lights), animation authoring, MegaCity demo |
| 5.6 | ~Oct 2025 | MEDIUM | Performance optimizations, bug fixes |
| 5.7 | Nov 2025 | HIGH | PCG production-ready, Substrate production-ready, AI assistant |
| **5.8** | **2026** | **HIGH** | Megalights production-ready, Lumen Lite (Beta), Substrate Toon NPR, Control Rig Dynamics |

## Major Changes from UE 5.3 to UE 5.8

### Breaking Changes
- **Substrate Material System**: New material framework (replaces legacy materials)
- **PCG (Procedural Content Generation)**: Production-ready, major API changes
- **Megalights**: New lighting system (millions of dynamic lights)
- **Animation Authoring**: New rigging and animation tools
- **AI Assistant**: In-editor AI guidance (experimental)
- **Symbol removal (5.8)**: 5.0–5.6 non-reflected, non-virtual symbols removed past
  the 2-version grace window — see `breaking-changes.md` `## 5.7 → 5.8`
- **`UE_LOG` → `UE_LOGF` (5.8)**: `UE_LOG` will eventually be deprecated; Epic ships
  `ConvertUELog.py` to migrate call sites

### New Features (Post-Cutoff)
- **Megalights**: Dynamic lighting at massive scale — **production-ready in 5.8**
- **Lumen Lite (Beta, 5.8)**: medium-quality GI via irradiance fields, ~2× faster
  than Lumen high quality; the new default for current-gen handhelds, also on PC
- **Substrate Materials**: Production-ready modular material system; **Substrate Toon
  Shading (experimental, 5.8)** for stylized NPR
- **PCG Framework**: Procedural world generation (production-ready in 5.7)
- **Enhanced Virtual Production**: MetaHuman integration, deeper VP workflows
- **Animation Improvements**: Better rigging, blending, procedural animation;
  **Control Rig Dynamics (5.8)** — lightweight particle-based solver
- **AI Assistant**: In-editor AI help (experimental)

### Deprecated Systems
- **Legacy Material System**: Migrate to Substrate for new projects
- **Old PCG API**: Use new production-ready PCG API (5.7+)
- **`UE_LOG`**: use `UE_LOGF` (5.8)
- **`PLATFORM_*` macros**: prefer `UE_PLATFORM_*` (5.8, optional migration)
- **LightWeightInstances**: marked deprecated (5.8)

## Verified Sources

Verified 2026-10-06 for the 5.8 pin:

- UE 5.8 release notes (fetched this run, versioned URL):
  https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8
  — release notes are the **only** source for the 5.7 → 5.8 facts in
  `breaking-changes.md`, `deprecated-apis.md` and `current-best-practices.md`
- UE 5 migration guide (fetched this run): https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-migration-guide
  — **this is a UE4 → UE5 guide, not a 5.7 → 5.8 guide**; it was checked and
  contains nothing specific to the 5.7 → 5.8 transition

Carried over from the previous pin (not re-verified this run):

- Official docs: https://docs.unrealengine.com/5.7/
- UE 5.7 release notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes
- What's new in 5.7: https://dev.epicgames.com/documentation/en-us/unreal-engine/whats-new
- UE 5.7 announcement: https://www.unrealengine.com/en-US/news/unreal-engine-5-7-is-now-available
- UE 5.5 blog: https://www.unrealengine.com/en-US/blog/unreal-engine-5-5-is-now-available

**NOT SOURCEABLE — 5.7 → 5.8 migration guide.** No official page titled for that
transition was found; the migration guide above is UE4 → UE5. The 5.8 release
notes' own "Upgrade Notes" section was used instead. A third-party guide
(`strayspark.studio`, "Upgrading Unreal Engine 5.7 to 5.8") was found via search
but returned **HTTP 403 (Cloudflare challenge)** when fetched, so **nothing from
it is recorded here**.
