@echo off

cd /D %~dp0


set EXEPATH=source\dotshader\exe\


copy %EXEPATH%SkinnedMesh_Release.exe distribution\PixelArtShader.exe
xcopy %EXEPATH%*.fxb distribution\ /Y
xcopy %EXEPATH%*.dll distribution\ /Y
xcopy %EXEPATH%*.h distribution\ /Y
xcopy %EXEPATH%*.teco distribution\ /Y

rd /S /Q distribution\bin
xcopy %EXEPATH%bin distribution\bin\ /S

rd /S /Q distribution\dest
md distribution\dest


rem === sample ===
rd /S /Q distribution\sample1
rd /S /Q distribution\sample2
rd /S /Q distribution\sample3
rd /S /Q distribution\sample4
xcopy %EXEPATH%sample1\* distribution\sample1\ /S
xcopy %EXEPATH%sample2\* distribution\sample2\ /S
xcopy %EXEPATH%sample3\* distribution\sample3\ /S
xcopy %EXEPATH%sample4\* distribution\sample4\ /S

pushd distribution
del /S /Q *.blend1
del /S /Q *.blend2
popd


rem === Blender plugin ===
rd /S /Q distribution\Blender_plugin
md distribution\Blender_plugin
xcopy source\blender\*.py distribution\Blender_plugin\ /Y


rem === manual ===
rd /S /Q distribution\manual
xcopy manual\_build\html distribution\manual\ /S


rem === etc. ===
xcopy distribution_src\* distribution\ /S /Y



pause
