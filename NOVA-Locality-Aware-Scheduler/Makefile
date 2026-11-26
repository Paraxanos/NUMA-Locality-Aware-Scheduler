# Makefile for NOVA Locality-Aware Co-Scheduler & AMD Opteron NUMA Benchmarks

CC = gcc
CFLAGS = -O3 -std=c99 -D_XOPEN_SOURCE=600 -fopenmp -Wall
INC = -Ivendor/mir-dev/src
LIB = -Lvendor/mir-dev/src -lmir-opt -lnuma -lm -pthread

BENCHMARKS = map matmul reduction jacobi sparselu
BINARIES = $(patsubst %,build/bench_%,$(BENCHMARKS))

.PHONY: all runtime benchmarks probe plot clean

all: runtime benchmarks

runtime:
	@echo "=== Building MIR Runtime with NOVA Co-Scheduler ==="
	cd vendor/mir-dev/src && scons -j4

benchmarks: $(BINARIES)

build/bench_%: benchmarks/numa/%.c benchmarks/numa/main.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(INC) $^ $(LIB) -o $@
	@echo "Built $@"

probe: benchmarks
	@echo "=== Running Full 200-Run Benchmarking Probe ==="
	python3 harness/run_matrix.py

plot:
	@echo "=== Generating Visualizations and Metrics Summary ==="
	python3 harness/plot_results.py

clean:
	rm -rf build/*
	cd vendor/mir-dev/src && scons -c
