@echo off
rem M0 baseline: stock Quake III Arena built from id's unmodified source.
rem QVMs run interpreted because the 2005 JIT is blocked by DEP (fixed in M1).
cd /d "%~dp0..\run\baseline"
start "" quake3.exe +set fs_basepath "%CD%" +set sv_pure 0 +set vm_game 1 +set vm_cgame 1 +set vm_ui 1 +set r_fullscreen 0 +set r_mode 6 %*
