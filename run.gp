reset
set xlabel 'term'
set xtics 0, 10
set ylabel 'time(nsec)'
set title 'Fibonacci'
set term png enhanced font 'Verdana,10'
set output 'result.png'
set key left

plot \
"time.csv" using 1:2 with linespoints linewidth 2 title "user", \
"time.csv" using 1:3 with linespoints linewidth 2 title "kernel", \
"time.csv" using 1:4 with linespoints linewidth 2 title "kernel to user"
