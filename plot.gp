# plot.gp - optional gnuplot charts from the CSVs in results/.
# usage (from the project root):  gnuplot scripts/plot.gp
set datafile separator ","
set terminal pngcairo size 1200,520 font "Helvetica,11"
set key outside bottom center horizontal
set grid ytics lc rgb "#dddddd"
set style fill solid 0.9 border -1

# 1) hit ratio per workload (grouped bars)
set output "results/hit_ratio.png"
set title "Hit ratio by workload and policy"
set ylabel "hit ratio (%)"
set yrange [0:100]
set style data histograms
set style histogram clustered gap 1
set xtics nomirror
plot for [c=2:8] "results/results_pivot.csv" using c:xtic(1) title columnheader(c)

# 2) GA convergence
set output "results/training_curve.png"
set title "Genetic algorithm convergence"
set xlabel "generation"
set ylabel "fitness (mean hit ratio)"
set autoscale y
set style data lines
set key inside bottom right vertical
plot "results/training_curve.csv" using 1:2 title "best" lw 3, \
     ""                           using 1:3 title "mean" lw 2, \
     ""                           using 1:4 title "worst" lw 1 dt 2

# 3) train vs test
set output "results/generalization.png"
set title "Mean hit ratio: train set A vs unseen test set B"
set xlabel ""
set ylabel "hit ratio"
set yrange [0:1]
set style data histograms
set key outside bottom center horizontal
plot "results/generalization.csv" using 2:xtic(1) title "train (A)", \
     ""                           using 3 title "test (B)"
