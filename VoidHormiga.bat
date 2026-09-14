@echo off
rem Launch Hormiga (build it first - see README.md).
rem
rem NAME THE DATABASE:   VoidHormiga.bat "C:\path\to\LON_01.state.json"
rem or drag the database file onto this .bat.
rem
rem With nothing named, Hormiga opens demo-org.json in the folder it was
rem started from - and double-clicked here, that folder is the SOURCE TREE.
rem That is how an organization's real data ended up beside the code twice
rem (before 2026-09-01, and again 2026-09-13). The app now says so in red in
rem its menu bar when it happens.
start "" "%~dp0build\bin\voidhormiga.exe" %*
