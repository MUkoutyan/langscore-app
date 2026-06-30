@echo off

cd ..\langscore-divisi

del CMakeCache.txt cmake_install.cmake

rd /S /Q .\CMakeFiles
rd /S /Q .\out
rd /S /Q .\build
rd /S /Q .\bin

mkdir build
mkdir bin
cd build

cmake.exe -G "Visual Studio 18 2026" .. -DCMAKE_C_COMPILER:STRING="cl.exe" -DCMAKE_CXX_COMPILER:STRING="cl.exe" -DCMAKE_BUILD_TYPE:STRING="Release" -DCMAKE_INSTALL_PREFIX:PATH="D:/Programming/Github/langscore-divisi/bin" "D:\Programming\Github\langscore-divisi"

cmake.exe --build .

cd %~dp0

echo Yes | xcopy ..\langscore-divisi\bin\divisi.exe .\bin\divisi.exe
echo Yes | xcopy ..\langscore-divisi\bin\rvcnv.exe .\bin\rvcnv.exe