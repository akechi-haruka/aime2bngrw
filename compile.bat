cd %~dp0
chcp 65001
C:\msys64\usr\bin\bash --login -c "DIR=%cd:\=\\\\%; cd $(cygpath $DIR) && ./compile.sh"

copy _build32\aime2bngrw\aime2bngrw.dll W:\apm\BB10\App\app\bngrw.dll