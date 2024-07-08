aime2bngrw
generic BNGRW I/O replacement DLL for using a physical Aime card reader for games that expect a BNGRW card reader
2024 Haruka
Licensed under the GPLv3.

Specialized for Synchronica, but can be used for any game.

--- Usage ---

* Place aime2bngrw.dll and aime2bngrw.ini in your game directory.
* Rename aime2bngrw.dll to bngrw.dll (and make a backup of the original)
* Make sure [bngrw] is set to enable=0 in segatools.ini.
* Synchronica: Make sure [side] is set correctly in aime2bngrw.ini.

See io42io3.example_sync.ini for an example config (FGO io4 to Synchronica WAJV)

--- Compiling ---

have msys2 installed at the default location and run compile.bat

or use CLion
