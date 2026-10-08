CC ?= clang
CFLAGS ?= -Wall -Wextra -O3 -std=c99 -Isrc
PYTHON ?= python3

SRC = src/tilepro64.c src/queue.c src/home_cache.c src/latency.c src/data_distribution.c src/task.c src/scheduler.c src/benchmark_map.c src/benchmark_vecmul.c
MAIN_SRC = src/main.c
TEST_SRC = tests/test_suite.c

all: bin/tilepro64 bin/test_suite

bin/tilepro64: $(SRC) $(MAIN_SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) $(MAIN_SRC) -o bin/tilepro64

bin/test_suite: $(SRC) $(TEST_SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) -o bin/test_suite

test: bin/test_suite
	@./bin/test_suite

experiment results: bin/tilepro64
	@mkdir -p results
	$(PYTHON) scripts/run_experiments.py
	$(PYTHON) scripts/plot_results.py

clean:
	rm -rf bin results/raw_results.csv results/tilepro64_results.png

.PHONY: all test experiment results clean
