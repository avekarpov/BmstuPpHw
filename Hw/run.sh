mpicc main.c -fopenmp -o $(basename "$PWD").out -O3 && 
mpiexec -n $1 --use-hwthread-cpus $(basename "$PWD").out > run_$(basename "$PWD")_$1.log 2>&1 &
