@echo off

rem この場所にシェーダー実行環境のコピー（exeディレクトリ）を生成する
rem コピーはバージョン管理外なので、自由に変更して動作させることができる
rem 既にコピーが存在する場合は、更新されたファイルだけをコピーする

rem 例外1: exe\config.txt は決して上書きコピーされない（ファイルが存在しなければコピーされる）
rem 例外2: destディレクトリの中身は決してコピーされない





cd /D %~dp0


rem set OPT=/D /I /E /Y /L
set OPT=/D /I /E /Y


set SRC_DIR=trunk\dotshader\exe
set DST_DIR=.\exe


rem 除外ファイルリスト
echo \dest\>temp.txt
echo exe\config.txt>>temp.txt
rem echo \pastel\>>temp.txt


xcopy %SRC_DIR% %DST_DIR% %OPT% /EXCLUDE:temp.txt


if not exist %DST_DIR%\dest mkdir %DST_DIR%\dest
if not exist %DST_DIR%\config.txt copy %SRC_DIR%\config.txt %DST_DIR%\


del temp.txt


rem pause
