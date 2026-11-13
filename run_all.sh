#!/usr/bin/env sh
# run_all.sh - build, test and regenerate every result file (Linux).
set -e
cd "$(dirname "$0")/.."
make -j4
make test
./build/cache_sim --train            # -> weights.txt, results/training_curve.csv
./build/cache_sim --evaluate         # -> results/generalization.csv, evaluation.csv
./build/cache_sim --compare-all      # -> results/results.csv, results_pivot.csv
./build/cache_sim --benchmark        # -> results/benchmark.csv
./build/cache_sim --trace --capacity 3 > results/trace_example.txt
if command -v gnuplot >/dev/null 2>&1; then gnuplot scripts/plot.gp; fi
echo "All done. See results/."
