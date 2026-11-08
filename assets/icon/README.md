# SDR Next icons

User-supplied artwork: `sdrnext.svg`, `sdrnext-small.svg` and the PNG exports in `png/`. These originals are preserved unchanged.

The application embeds all PNG sizes in Qt resources. Windows also embeds `sdrnext.ico` as an executable resource. The ICO contains the original 16–256 px PNG payloads; the 512 px export is used by Qt.

Regenerate the ICO after replacing PNG exports:

```sh
python scripts/generate-icon-ico.py
```

The script uses the Python standard library and performs no resizing or pixel edits. The checked-in ICO avoids adding Python to the normal build prerequisites. Resource compilation and visual verification are scheduled for the next application build.
