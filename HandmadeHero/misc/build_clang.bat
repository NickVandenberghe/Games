@echo off

set CommonCompileFlags=-g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-missing-field-initializers -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -fdiagnostics-absolute-paths -target x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fdelayed-template-parsing
set CommonLinkerFlags=-luser32 -lgdi32 -lwinmm

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

del *.pdb > NUL 2> NUL
clang++ %CommonCompileFlags% ..\code\handmade.cpp -shared -o handmade.dll -Wl,--out-implib,handmade.lib -Wl,--export-all-symbols
clang++ %CommonCompileFlags% ..\code\win32_handmade.cpp -o win32_handmade.exe %CommonLinkerFlags%
popd
