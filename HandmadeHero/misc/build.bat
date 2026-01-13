@echo off

set CommonCompileFlags=-MTd -nologo -EHsc -EHa- -Oi -Od -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -Z7 -FAsc 
set CommonLinkerFlags= -incremental:no user32.lib Gdi32.lib Winmm.lib

IF NOT EXIST .\build mkdir .\build
pushd build

del *.pdb > NUL 2> NUL
cl %CommonCompileFlags%  ..\code\handmade.cpp -Fmhandmade.map /LD /link /EXPORT:GameGetSoundSamples /EXPORT:GameUpdateAndRender -incremental:no /PDB:handmade_pdb_%random%.pdb
cl %CommonCompileFlags%  ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags% 
popd
