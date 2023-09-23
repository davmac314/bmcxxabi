set -eu
make -C .. OUTDIR="${PWD}"
g++ -Wno-inaccessible-base -o tests tests.cc -L. -lcxxabi
echo "Now run ./tests"
