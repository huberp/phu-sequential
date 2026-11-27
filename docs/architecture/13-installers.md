# 13. Installer Design

**Status:** planned, not implemented. Implements [spec §25](../../spec/23-installers.md).
The spec governs behavior. This design starts with phu-sequential but keeps the
packaging code independent of its DSP and UI so other JUCE plugins can reuse it.

## 13.1 Boundaries and Inputs

Use one small CMake packaging module and platform-specific packaging files:

| Planned location | Responsibility |
|---|---|
| `cmake/PhuPackaging.cmake` | Product inputs, bundle staging, CPack configuration |
| `packaging/Product.cmake` | Product identity and release target/baseline configuration |
| `packaging/windows/` | NSIS script and runtime configuration |
| `packaging/macos/` | Minimal productbuild resources, only if needed |
| `packaging/linux/` | Distro build and package verification helpers, only if needed |
| `.github/workflows/` | Build, validate, package and publish release assets |

No separate shared repository, framework, installer application or plugin code
changes are needed. Initially copy the small packaging module and platform files
to another repository; extract a shared package only when actual reuse justifies it.

Expose `phu_configure_packaging(TARGET <shared-target> PACKAGE_ID <id>
PRODUCT_NAME <name> VENDOR <name> FORMATS <formats...> VERSION <tag>
GUIDE_URL <url> LEGACY_PATHS <paths...>)`. Enable it only for packaging builds.
The stable, platform-valid ID is product-specific and does not include a version
or architecture; use it for the Windows uninstall key, macOS package receipt
and Linux package names. Legacy paths are optional and limited to known product
bundle locations expressed relative to approved OS-resolved plugin roots.
Require valid inputs and fail explicitly.

Derive format targets and bundle names from JUCE target metadata, not duplicated
paths. For phu-sequential these are `phu-sequential_VST3` and, on macOS,
`phu-sequential_AU`.

After `juce_add_plugin`, register install rules for entire built bundles using
JUCE's `JUCE_PLUGIN_ARTEFACT_FILE` property on each format target, accounting
for configuration-specific generator expressions. Do not select the first
recursive `*.vst3` match. `install()` does not build targets: build and validate
the requested format targets before packaging. Use relative install destinations
and isolated staging roots, never write into host plugin folders during a build.
Use generator-specific CPack configuration for the different package prefixes.
Handle all CMake generators, including multi-configuration Visual Studio/Xcode.

## 13.2 Platform Packaging

| Platform | Build output and install mechanism |
|---|---|
| Windows x64 | One NSIS `.exe`; CMake stages the VST3 bundle, then NSIS installs it. |
| macOS x64, arm64 | One CPack `productbuild` `.pkg` per architecture containing both VST3 and AU. |
| Linux x64, arm64 | CPack DEB and RPM packages plus a portable ZIP per architecture. |

Use CPack for native packages where it fits. Use a short standalone NSIS script
rather than copying CPack's general-purpose NSIS template: the plugin goes in a
shared VST3 directory while the uninstaller and registry entry are product-owned.
Keep that script generic and feed it the staged bundle and product inputs.
No custom Linux installer UI or background dependency downloader is introduced.

**Windows.** Resolve the 64-bit Common Files folder through Windows/NSIS and
install the VST3 bundle under `VST3\PHU\`. Store the uninstaller under a
product-owned directory outside the shared VST3 tree and register one stable
Apps & Features entry. Do not recursively remove the shared `PHU` folder.

Before writing product files, read the installed x64 VC runtime's `Installed`
and numeric `Major`, `Minor`, `Bld`, `Rbld` values in the correct registry view.
Compare against the minimum runtime for the actual MSVC toolset. At packaging,
obtain the official x64 redistributable at a pinned version, verify its hash and
Microsoft signature, check it meets that minimum, and embed it. Updating the
pin is an explicit maintenance change; do not download on the user's machine.
Check redistribution terms for the build environment.

If the runtime is missing or too old, explain the prerequisite and ask permission;
No cancels before product installation. Yes runs the embedded redistributable with
`/install /passive /norestart`, waits for completion and rechecks the registry.
Treat a newer runtime as already satisfied. Handle success, reboot-required,
failure and cancellation distinctly; if availability cannot be established, stop
and report the error and log location. If a reboot is required, report it and do
not claim the plugin is ready until that requirement is met. Show a final wizard
summary with product path, runtime already present/installed, and reboot status;
on failure or cancellation show the corresponding result, not success.

**macOS.** Stage relative paths under `/` so productbuild installs VST3 to
`Library/Audio/Plug-Ins/VST3/PHU/` and AU directly to
`Library/Audio/Plug-Ins/Components/`. Use stable package identifiers and native
bundle replacement semantics; test that old bundle contents are replaced on
upgrade. Supply a productbuild distribution conclusion resource identifying both
formats and stating that no additional dependencies were installed. The `.pkg`
and bundles are initially unsigned and unnotarized; document Gatekeeper's
limitations without implying a trust warning will disappear. Document removing
only this product's bundles and then its receipt; `pkgutil --forget` does not
delete payload files.

**Linux.** Build DEBs in compatible Debian/Ubuntu environments and RPMs in a
compatible Fedora environment, for each supported architecture. Use distro
dependency tooling on the actual VST3 ELF and include dynamically loaded runtime
libraries that automatic analysis misses. Inspect package metadata and test
dependency resolution; do not translate package names between distros by guesswork.
Use `/usr/lib/vst3/PHU/` for DEB and `/usr/lib64/vst3/PHU/` for Fedora RPM.
Create the portable ZIP from a documented compatible build, with the bundle at
its root; document copying it to `~/.vst3/PHU/` and any runtime prerequisites.
Package managers own upgrades/removal and dependency decisions. Document
`apt install ./<package>.deb` and `dnf install ./<package>.rpm`; do not use
unattended flags in user instructions. Native frontend prompts and final
transaction summaries vary; neither is guaranteed and no wrapper is provided.

## 13.3 Lifecycle and Safety

- Keep each product's package and uninstall identities stable across releases.
  A reinstall or upgrade replaces only its bundle(s), without duplicates or
  obsolete files. Never remove presets, other products or shared runtimes.
  Fail clearly on file locks, permissions or package conflicts; preserve the
  previous working install where possible and report any incomplete state.
- On Windows, check the exact product-specific legacy locations from the old
  release instructions. Show detected paths and offer removal only with explicit
  confirmation and only when they can be established as unmanaged. Defer removal
  until the new product installation succeeds. Never delete a package-manager-owned
  or uncertain copy; explain manual cleanup instead.
  Refuse to overwrite an existing destination of uncertain ownership. Resolve
  paths safely and do not follow links outside the expected bundle.
- On macOS and Linux, do not delete legacy user copies or prompt from package
  scripts. The guide explains how to locate duplicates and remove only the
  intended product. Windows decline or cancellation leaves existing copies alone.
- The final Windows summary reports cleanup and dependency outcomes. Native macOS
  reports successful installation and no extra dependencies. Linux delegates
  reporting to its package manager, as specified above.

## 13.4 Versioning and Releases

Use the release tag as the one source of release identity, not the current
hard-coded `project()` version. Initially accept `vM.m.p` and `vM.m.p-rcN`,
with non-negative numeric components `M`, `m`, `p` at most 9999, no leading
zeroes except `0`, and `N` in 1..998. Reject other tags before producing or
publishing assets, with an actionable error. Preserve the full tag in the
asset filename and user-visible package metadata.

For native ordering, use `M.m.p~rcN` for DEB/RPM prereleases and `M.m.p` for
final releases. For macOS package receipts use the numeric version
`M.m.(p*1000+N)` for an RC and `M.m.(p*1000+999)` for final. Windows stores
the full tag as the displayed version and compares `(M,m,p,N)` for RCs and
`(M,m,p,999)` for finals before allowing an upgrade; reject downgrades.
Supply `M.m.p` before CMake's `project()` call and use it for JUCE's numeric
plugin version, retaining the existing version as the non-release default. Test
the actual native comparators and replacement behavior, including RC-to-final;
if any platform cannot represent or order a version correctly, fail the release.
Package metadata, asset name and plugin version must all refer to the same tag.

Generate the PDF guide before packaging or publish it as a clearly linked
separate release asset. Update release notes and the user guide for native
installation, removal, unsigned warnings and rescan. Do not inject a PDF into
native packages after they are built.

Replace the five-ZIP assumption in `release.yml` with an explicit expected-asset
manifest. Name assets with product, complete tag, OS, architecture, format and,
where needed, Linux distro/baseline. Build DEB per Debian/Ubuntu baseline and
RPM per Fedora baseline when necessary; no overwriting identically named assets.
Use the portable ZIP per architecture from a documented compatible baseline.
Publish only when all required assets, package checks and validation succeed.
Keep ordinary CI builds independent of packaging tools.

### Release workflow structure

Keep [release.yml](../../.github/workflows/release.yml) as the orchestrator and
the existing reusable platform workflows as the build entry points:

1. **Prepare:** validate the tag, resolve its commit SHA, and produce the
   platform/architecture/baseline matrices and expected asset names from one
   release configuration. The PDF guide is generated once. Validate before
   creating a tag in a manual run.
2. **Build and package:** call the Windows, macOS and Linux reusable workflows
   with matrix inputs, the resolved SHA and version. Each checks out that SHA,
   builds, tests, packages via the shared CMake/platform logic, validates its
   packages and uploads uniquely named assets plus test results. Linux matrix
   entries specify the distro baseline and architecture; designate exactly one
   portable ZIP producer per architecture. Do not copy packaging commands into
   the orchestrator or rebuild a different binary for packaging.
3. **Verify:** download the artifacts from this run, compare the exact set with
   the expected manifest, reject missing, duplicate or unexpected assets, and
   verify package identity and test results. Generate a SHA-256 manifest for
   the validated release assets and expose it with the artifacts for review.
4. **Approve and publish:** after verification, require a protected GitHub
   environment approval before publication. Reviewers download those artifacts,
   perform and record the required manual DAW checks against their SHA-256
   hashes, and approve only if all required results pass. Publish exactly the
   verified artifacts and checksum manifest; do not rebuild or alter them after
   approval. A rerun that produces different assets requires fresh validation
   and approval. Configure the environment and reviewers as a release
   prerequisite; if the protection is unavailable or not configured, do not
   publish until an equivalent enforced gate is established.

Check the environment's required-reviewer configuration before entering the
approval job, and verify an approval for this run before publishing; fail
closed if either check is unavailable. GitHub can create a referenced
environment without protection rules, so referencing its name alone is not
sufficient. Recheck the downloaded assets against the approved checksum
manifest immediately before publishing.

The normal [build.yml](../../.github/workflows/build.yml) continues calling the
same platform workflows without release packaging. Add only the inputs needed
for release packaging and Linux baselines; keep runner/tool installation and
platform-specific commands in their respective workflows or packaging helpers.
Keep product identity and release targets in `packaging/Product.cmake`, consumed
by the plugin's CMake setup and packaging module. A small CMake script reads that
configuration to emit the matrices and expected asset names for the prepare
job. Derive bundle paths from JUCE and keep version mapping in the packaging
module, not duplicated in workflow steps.

Use read-only repository permissions for build jobs and grant `contents: write`
only to jobs that create a tag or publish. Validate release inputs before using
them; pass them as data rather than interpolating them into shell scripts.
Artifact names include platform, architecture and baseline so parallel jobs
cannot collide. Reusing the workflows in another repository also requires the
corresponding packaging files and product configuration there; a reusable
workflow alone does not supply those files.

## 13.5 Validation and Implementation Order

1. Add product inputs, deterministic bundle staging and version validation;
   verify missing targets, configurations, invalid versions and staged contents.
2. Add Windows NSIS, macOS productbuild and Linux DEB/RPM/ZIP packaging. Inspect
   each package's paths, architecture, metadata, complete bundles and dependencies.
3. Test clean install, reinstall, upgrade including RC-to-final, removal, data
   preservation and another installed PHU product. Test Windows runtime
   missing/current/newer, refusal, failure and reboot; test legacy detection,
   confirmation and uncertain ownership. Exercise Linux dependency resolution.
4. On each supported OS/architecture and Linux distro baseline, validate the
   **installed** plugin with a suitable scanner/host; check both macOS formats
   and VST3 subfolder discovery in representative DAWs. Record manual checks
   where CI cannot run a DAW; do not treat a successful build as an install test.
5. Wire release jobs and guide updates; make missing assets and failed required
   checks block publication. Run Windows installer tests in disposable machines;
   never test destructive install/uninstall operations against a shared host.

References: [JUCE CMake API](../../JUCE/docs/CMake%20API.md),
[CPack](https://cmake.org/cmake/help/latest/module/CPack.html),
[Microsoft runtime deployment](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files),
[VST3 locations](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Locations+Format/Plugin+Locations.html).
