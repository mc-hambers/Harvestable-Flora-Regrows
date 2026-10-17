# Harvestable Flora Regrows

Skyrim SE/AE mod — source version **1.1.1**.

## Complete source

[Download all corresponding source and build files](Harvestable-Flora-Regrows-source.zip). The archive includes the mod implementation, configuration, scripts, dependency source, build instructions, and original license notices. Native mod code is also browsable in [src/](src/).

## Precompiled CommonLib

[Download the matching precompiled CommonLib](CommonLibSSE.lib.xz). This is a development dependency; do not install it into Skyrim.

Extract the complete source archive, then run:

```text
python Restore-CommonLib.py <path-to-extracted-source-folder>
```

Keep [Restore-CommonLib.py](Restore-CommonLib.py) beside CommonLibSSE.lib.xz. The helper decompresses the library to the correct project path and verifies its SHA-256. Python 3.11 or later is required. Follow the original project build instructions for the MSVC compiler, SDK, and Papyrus tools. CMake projects normally compile CommonLib from the included source; the precompiled library is additionally provided for development.

## License

Copyright (C) 2026 Maxmc.

The original mod code is free software: you may redistribute it and/or modify it under the GNU General Public License, version 3 or (at your option) any later version.

This program is distributed WITHOUT ANY WARRANTY, including the implied warranties of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE.txt](LICENSE.txt) for the full terms. Third-party code retains its original copyrights, license terms, and applicable exceptions, preserved in the complete source archive.
