#!/bin/sh
# runs every tests/*.mc with and without optimization and diffs against the .out file
set -e; make -s; pass=0; fail=0
for f in tests/*.mc; do
  for m in "" "--no-opt"; do
    if ./minicc run "$f" $m | diff -q - "${f%.mc}.out" >/dev/null; then pass=$((pass+1)); else echo "FAIL: $f $m"; fail=$((fail+1)); fi
  done
done
echo "passed $pass, failed $fail"; [ $fail -eq 0 ]
