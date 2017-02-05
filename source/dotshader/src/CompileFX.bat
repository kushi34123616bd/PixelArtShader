@echo off

echo %time%

fxc.exe skinnedmesh.fx /T fx_2_0 /Fo ..\exe\skinnedmesh.fxb

rem fxc.exe skinnedmesh.fx /T fx_2_0 /Gfp /Fo ..\exe\skinnedmesh.fxb /Fc hoge.txt
rem fxc.exe skinnedmesh.fx /T fx_2_0 /Od /Fo ..\exe\skinnedmesh.fxb /Fc hoge.txt

echo %time%

pause
