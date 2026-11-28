# 13. Installer Design

**Status:** local Windows packaging implemented; installer behavior has not been
tested. Other platforms and release integration are planned. Implements
[spec §25](../../spec/23-installers.md).
The spec governs behavior. Keep the packaging small and reusable for other PHU
plugins without introducing a separate framework.

## 13.1 Scope and Build Locations

- Use a separate CMake packaging project and platform-specific packaging files;
  do not add installer logic to the existing application build. Pass it the
  already-built bundles and release version explicitly. Separate product name,
  stable package identities and guide location from shared logic. Other
  repositories are not changed.
- Use the existing [release workflow](../../.github/workflows/release.yml) and
  its reusable platform workflows. Build Windows, macOS and Linux packages on
  their respective GitHub-hosted runners; use compatible Linux distribution
  environments on those runners.
- Local packaging is supported only for the host OS and architecture. Do not
  add cross-build tooling for local installer builds.
- Do not add automated installer tests, mandatory manual installer checks or a
  separate installer-test approval gate for now. Existing plugin tests and
  validation in the build workflows remain in place. Building a package is not
  proof of successful installation or host discovery.

## 13.2 Packaging

The separate packaging project consumes complete, already-built JUCE bundles.
The caller supplies the application build directory and builds the required
plugin targets before packaging. Windows WiX packages the x64 binary from the bundle;
use CMake install rules for staging where appropriate on other platforms.
Do not install into system plugin folders during a build. Use CPack where
appropriate.

The local Windows packaging entry point is `packaging/CMakeLists.txt` and its
`installer-windows-x64` preset. It accepts `PHU_PLUGIN_BUILD`,
`PHU_RELEASE_TAG` and `PHU_REDIST`; it reads the VST3 bundle and compiler
metadata from the application build and checks the plugin version and pinned
runtime before invoking WiX. See the [build instructions](../../README.md#local-windows-installer).
No application CMake files or release workflows are changed.

On Windows, Burn checks the runtime registry to decide whether to offer the
redistributable. Afterward, the MSI independently checks the versions of the
x64 runtime DLLs before installing; consent alone does not satisfy this check.

For CMake-based packaging, reuse the existing OS, architecture and build-type
preset mixins where practical. Add opt-in installer configure/build presets only
where they simplify local and runner builds; avoid duplicating application settings.
Use separate build directories when packaging configuration or Linux baselines
would otherwise conflict. Keep ordinary application presets unchanged. CPack
package presets are optional: they require preset schema version 6 or later
(the current file uses version 5), so upgrade the schema only if useful.
Presets must not imply support for local cross-building.

| Platform | Package | Installed bundles |
|---|---|---|
| Windows x64 | WiX Burn `.exe`, containing the plugin MSI and VC++ runtime | Common Files `VST3\PHUplugins\phu-sequential.vst3` |
| macOS x64, arm64 | CPack productbuild `.pkg` | `/Library/Audio/Plug-Ins/VST3/PHU/` and `/Library/Audio/Plug-Ins/Components/` |
| Linux x64, arm64 | CPack DEB and RPM, plus portable ZIP | DEB: `/usr/lib/vst3/PHU/`; RPM: `/usr/lib64/vst3/PHU/` |

**Windows.** Use WiX's standard bootstrapper where it meets the requirements;
keep any required custom UI or logic minimal. Resolve 64-bit Common Files through
Windows. The flat single-file VST3 format is deprecated by Steinberg and may not
be recognized by all hosts; bundle resources are not installed. Bundle the
official x64 VC++ redistributable for offline installation,
pin and verify its download, and check its version against the compiler's runtime
requirement. Detect an existing compatible or newer runtime. If it is missing or
too old, ask permission before installing it; declining cancels the plugin
installation. Report runtime failures and reboot requirements, and include the
dependency outcome in the final summary. Do not uninstall a shared runtime.
The initial install page identifies the plugin and destination, shows the
runtime status and provides Install and Cancel controls.
Use stable WiX bundle and MSI upgrade identities, and let Windows Installer
manage product files and uninstall registration.

**macOS.** Install both formats together using stable package identities and
bundle replacement semantics. The native conclusion identifies the formats and
states that no additional dependencies were installed. Document removal of the
product bundles and receipt separately; forgetting a receipt does not remove files.

**Linux.** Build against compatible Debian stable, Ubuntu LTS and Fedora
baselines for each supported architecture. Use distribution tooling to derive
dependencies from the built plugin and include necessary dynamically loaded
libraries. Produce the portable ZIP from a documented compatible baseline;
document manual installation to `~/.vst3/PHU/` and its compatibility limits.
Package managers handle installation, removal, dependencies and their own UI.

All installers are initially unsigned, and macOS packages are not notarized.
Document SmartScreen and Gatekeeper limitations and safe user instructions.

## 13.3 Lifecycle and Existing Copies

Reinstall and upgrade replace the product's installer-owned files without
duplicates or obsolete files. Removal must preserve presets, other products and
shared dependencies. Fail clearly on conflicts and errors.

Do not detect or remove legacy manual copies automatically. Document how to
identify and remove them manually and warn about duplicate discovery.

## 13.4 Versioning and Release Workflow

Use the release tag for plugin and package identity. Accept `vM.m.p` and
`vM.m.p-rcN`, with no leading zeroes except `0`, `M` and `m` in 0..255,
`p` in 0..64, and `N` in 1..998. Reject unsupported tags before packaging.
For Windows MSI use `M.m.(p*1000+N)` for an RC and `M.m.(p*1000+999)`
for a final release: MSI compares only three fields, with limits 255, 255 and
65535. Use the same numeric mapping for the macOS receipt, and
`M.m.p~rcN` / `M.m.p` for Linux. Preserve the full tag in asset names and
user-visible release information; use `M.m.p` as the JUCE plugin version.
Keep package upgrade identities stable across releases.

The existing release workflow orchestrates the platform builds, guide generation
and publication. Extend the existing reusable build workflows with opt-in
packaging; ordinary CI remains independent of packaging tools. Upload uniquely
named assets per OS, architecture, format and Linux baseline. Publish only after
the required build jobs succeed and all expected assets exist. Publish the PDF
guide as a separate release asset or otherwise make it readily accessible.
Pass release inputs as data and grant write permissions only where needed.

## 13.5 WiX License and Maintenance

WiX source is under the [Microsoft Reciprocal License (MS-RL)](https://github.com/wixtoolset/wix/blob/v7.0.1/LICENSE.TXT).
It permits royalty-free commercial use, including donationware. However, that
does **not** make every official binary release unconditionally free:

- The [WiX v7.0.1 binary EULA](https://github.com/wixtoolset/wix/blob/v7.0.1/OSMFEULA.txt)
  exempts users with annual gross revenue below US$10,000. It requires a
  maintenance fee for covered revenue-generating use at or above that amount,
  unless another stated exemption applies. Confirm applicability before using
  the official binaries; donationware alone does not establish an exemption.
  The v6 EULA differs, so check the terms of the exact selected version.
- WiX v7 requires explicit EULA acceptance, including in CI. Do not configure
  acceptance before the maintainer has reviewed the applicable terms. Pin the
  chosen tool and extension versions; recheck their terms when upgrading.
- If distributing WiX code, including code embedded in a Burn bootstrapper,
  retain its notices and provide recipients the corresponding source and a copy
  of MS-RL as required by the license. Include the relevant notices and license
  with the distribution, and make the matching WiX source available to recipients.
  Independent product files do not automatically become MS-RL-licensed.
  Review the actual tool and extensions used before publishing.
- MS-RL grants no trademark license and disclaims warranties; patent-retaliation
  and other conditions also apply. The VC++ redistributable has separate Microsoft
  redistribution terms. This summary is not legal advice.

If the applicable WiX binary terms are not free for this project, use a
compliant MS-RL source build or revisit the Windows tool choice rather than
assuming an exemption or accepting a fee.

References: [WiX Burn](https://docs.firegiant.com/wix/tools/burn/),
[WiX maintenance fee](https://docs.firegiant.com/wix/osmf/),
[MSI version limits](https://learn.microsoft.com/en-us/windows/win32/msi/productversion),
[CPack](https://cmake.org/cmake/help/latest/module/CPack.html),
[Microsoft runtime deployment](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files).
