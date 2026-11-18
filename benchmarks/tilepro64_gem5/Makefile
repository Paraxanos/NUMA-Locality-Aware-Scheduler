CC ?= gcc
CFLAGS ?= -O3 -Wall -pthread -std=c11

TARGETS = calibrate_latency bench_gem5

all: $(TARGETS)

calibrate_latency: calibrate_latency.c
	$(CC) $(CFLAGS) -o $@ $<

bench_gem5: bench_gem5.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) *.o
