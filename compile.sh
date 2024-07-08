# idk linux lol

export LANG='C.UTF-8' # fixes console output
export TERM=xterm-mono # also something with console output
set +e # something about error handling
meson --cross cross-mingw-32.txt _build32  # compile x86
meson --cross cross-mingw-64.txt _build64  # compile x64
ninja -C _build32 && ninja -C _build64 # compile again?
if [ $? != 0 ] ; then # if compilation failed, exit
  exit 1
fi
find ./ -type f -name "*.dll" -exec strip -s {} \; # prevent path leaks
