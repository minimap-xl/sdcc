set terminal pdf
set output output_file

set yrange [-5:9]
plot input_file using 1:($2-2) with fsteps title "rx-level", \
             "" using 1:3 with fsteps title "quantum-m-cnt", \
             "" using 1:($4-4) with fsteps title "sync-inhibit"

set output
