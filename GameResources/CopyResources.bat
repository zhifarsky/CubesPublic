::call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set SOLUTION_FILE="..\FirstOpenGL.sln"
DevEnv /Rebuild Debug /Project GameResources  %SOLUTION_FILE%