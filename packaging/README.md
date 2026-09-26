# Packaging assets

Platform files used by release packaging (ADR-0012). They are generated
from the brand book; the master icon is in [`assets/icon/`](../assets/icon/).

| Path | Used for |
|---|---|
| `windows/sdrnext.ico` | Executable icon (16–256 px), embedded as a Windows resource |
| `windows/msix/` | MSIX package logos (`Square44x44Logo`, `Square150x150Logo`, `StoreLogo`) |
| `linux/hicolor/` | Freedesktop icon theme (`/usr/share/icons/hicolor/<size>/apps/sdrnext.*`) |
| `linux/sdrnext.desktop` | Desktop entry (`/usr/share/applications/`) |

Installers that use these files are planned (MSIX or MSI, `.deb`, `.rpm`,
AppImage or Flatpak).
