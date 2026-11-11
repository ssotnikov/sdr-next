# Third-party notices — preview distribution

The project's own sdr-app code is distributed under the MIT license (LICENSE). Third-party components retain their own terms; the complete executable package is not declared to be entirely MIT-licensed.

## Qt 6.10.0

The application uses dynamically linked Qt Core, Gui, Qml, Quick, QuickControls2/Fusion, Multimedia, and their required dependencies. The selected open-source licensing option is subject to the licenses of the respective modules (including LGPLv3); the source code for the exact version is available in the [Qt download archive](https://download.qt.io/archive/qt/6.10/6.10.0/). The primary license texts are included in licenses; the complete set of third-party components is determined from the Qt SBOM and the actual DLL/SO files in each package.

Qt libraries must remain replaceable with compatible builds. The application source code and CMake instructions allow the application to be rebuilt and relinked. The packaging process does not link Qt statically. The terms applicable to the selected modules and their transitive components must be reviewed before the final release.

The Windows build uses the native multimedia backend. The packaging script disables automatic FFmpeg deployment and uses the operating system's ICU library when the deployed copy matches the system file. The package inventory identifies the Microsoft redistributables included with each build.

## SoapySDR and hardware drivers

SoapySDR 0.8 is an external runtime accessed through its public C API; upstream: [pothosware/SoapySDR](https://github.com/pothosware/SoapySDR), BSL-1.0. Users install SoapyHackRF/SoapyRTLSDR/SoapyAirspy, USB libraries, and device SDKs separately. These components are not copied by the packaging process and are not covered by the application's MIT license. Driver terms may differ, including copyleft licenses; distributing them together requires a separate review.

## Build tools and compiler runtime

The Qt SDK, CMake, Ninja, Cargo/Rust, and build environments are not included in the executable package. When MinGW is used, its runtime components are distributed under their own terms, including applicable runtime exceptions; the presence and text of these terms are checked for each specific preview build.

The source repository includes a modified SoapySDR vcpkg port recipe derived from microsoft/vcpkg, copyright Microsoft Corporation, under the MIT license. See cmake/vcpkg-ports/soapysdr/LICENSE.txt. This recipe license does not replace the SoapySDR library's BSL-1.0 license.

## Status

This notice does not indicate that the audit is complete. The final G6 gate requires checking the actual binary dependencies, all applicable notices, and license texts. `package-manifest.json` contains filenames, sizes, and SHA-256 hashes, but does not by itself replace a complete SBOM covering licenses and dependencies.

## IBM Plex fonts

The application embeds unmodified IBM Plex Sans (Regular, Medium, SemiBold) and IBM Plex Mono (Regular, Medium) fonts. Copyright and SIL Open Font License 1.1: see assets/fonts/OFL.txt in source distributions and licenses/IBM-Plex/OFL.txt in application packages. Font filenames and SHA-256 identifiers are listed in assets/fonts/README.md.
