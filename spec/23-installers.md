# Installer Specification

## 25. Installers and Distribution

### 25.1 Scope

- Provide installers for phu-sequential's existing release targets: Windows x64 VST3,
  macOS x64 and arm64 VST3 plus AU, and Linux x64 and arm64 VST3. Windows arm64
  is not a release target merely because a build preset exists.
- Design the packaging logic for reuse by other PHU plugins, with product-specific
  names, versions, bundles and dependencies separated from shared logic. This spec
  does not require changing other repositories.
- Build and publish the installers through the existing release workflow. Replace
  the current Windows and macOS ZIP releases; retain a portable Linux archive as
  a fallback alongside the Linux packages. Include or make the user guide readily
  accessible for each distribution.
- Build release installers on the corresponding GitHub Actions platform runners.
  Local builds support only the developer machine's own platform and architecture;
  cross-building installers locally is out of scope.
- Do not add installer-specific automated or mandatory manual tests for now.
  Existing plugin build and test workflows remain unchanged.

### 25.2 Packaging and Installation

- Use CMake install rules and CPack where practical, with small platform-specific
  packaging logic: WiX for the Windows installer, Apple productbuild for the
  macOS `.pkg`, and DEB and RPM packages on Linux. CPack alone does not provide
  dependency bootstrapping or guarantee correct platform-specific destinations.
  Use WiX only under terms that permit free use for this project; see the
  license and maintenance-fee conditions in the installer architecture.
- Install system-wide, requesting administrator privileges where needed. Install
  complete bundles except on Windows, where the x64 binary is installed directly:

  | Platform | Destination |
  |---|---|
  | Windows VST3 | Standard 64-bit Common Files `VST3\PHUplugins\phu-sequential.vst3` file |
  | macOS VST3 | `/Library/Audio/Plug-Ins/VST3/PHU/` |
  | macOS AU | `/Library/Audio/Plug-Ins/Components/` directly |
  | Linux DEB VST3 | `/usr/lib/vst3/PHU/` |
  | Linux RPM VST3 | `/usr/lib64/vst3/PHU/` |

  The single-file Windows format is deprecated by Steinberg and may not work in
  all hosts. Resolve the Windows standard folder through the OS, not a hard-coded drive.
  Do not put AU bundles in a `PHU` subfolder without a separate compatibility
  decision.
- Install both macOS formats together. The Linux portable archive supports manual
  per-user installation to `~/.vst3/PHU/` and documents its compatibility limits.
- Support reinstall and upgrade without leaving duplicate installer-owned copies.
  Uninstall removes only the product's installed files and metadata; it must not
  remove other PHU products, shared runtimes, or user presets and data. Provide
  a Windows uninstaller, use the package manager on Linux, and document safe
  macOS removal, including package receipt handling. Do not claim that removing
  a macOS receipt removes files.

### 25.3 Dependencies and Compatibility

- Bundle the official Microsoft Visual C++ x64 Redistributable in the Windows
  installer so installation works offline. Check for an installed compatible
  runtime at least as recent as the MSVC build tools; install the bundled runtime
  only if needed. Never downgrade or uninstall a shared runtime. Respect Microsoft's
  redistribution terms, verify the bundled download, and keep it current.
- On Windows, show the plugin, destination and dependency status before installation,
  with Install and Cancel controls. The user can decline a required dependency;
  cancel the plugin installation if it is declined. Present a final summary including dependency
  status. On macOS, use the native installer summary, including dependencies.
  On Linux, use the package manager's native confirmation and result reporting;
  dependency prompts and final summaries vary by frontend and are not guaranteed.
- Report dependency installation failures and reboot requirements explicitly; do
  not report a usable installation when a required runtime is unavailable.
- Provide Linux DEB and RPM dependency metadata for the actual runtime libraries;
  package managers may require network access to obtain missing dependencies.
  Target current Debian stable, Ubuntu LTS and Fedora on both supported
  architectures where those distributions support them. Build against compatible
  baselines and declare dependencies for the actual runtime libraries; changing
  the package extension alone does not establish compatibility. Other distributions
  are best-effort via the portable archive.
- Publish unsigned Windows installers and unsigned, non-notarized macOS packages
  initially. Explain SmartScreen and Gatekeeper limitations and provide safe,
  current user instructions. Do not promise warning-free installation.

### 25.4 Existing Installations and Releases

- Do not automatically detect or remove legacy manual copies. Document how to
  identify and remove old copies manually and warn about duplicate discovery.
- Release assets identify product, version, OS, architecture and package format.
  Preserve release and prerelease identity in naming and package metadata;
  reject unsupported version mappings rather than silently mislabeling a package.
- Fail packaging or publication when a required build or installer asset is
  missing or a packaging command fails. A successful build does not prove that
  an installer or plugin works in a host.

References: [VST3 locations](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Locations+Format/Plugin+Locations.html),
[Microsoft VC++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist),
[WiX Toolset](https://docs.firegiant.com/wix/),
[CPack productbuild](https://cmake.org/cmake/help/latest/cpack_gen/productbuild.html),
[Apple Developer ID](https://developer.apple.com/developer-id/).
