CC ?= clang
CFLAGS ?= -Wall -Wextra -Werror -O3 -pthread
INCLUDES = -Itests/mock_mir -Ipatches/nova

NOVA_SRCS = patches/nova/nova_shadow_index.c \
            patches/nova/nova_coordinator.c \
            patches/nova/nova_cost.c \
            patches/nova/nova_dealer.c \
            patches/nova/nova_stealer.c \
            patches/nova/mir_arch_sim.c

all: tests/test_nova_queue_race tests/test_nova_concurrency

tests/test_nova_queue_race: tests/test_nova_queue_race.c
	$(CC) $(CFLAGS) $< -o $@

tests/test_nova_concurrency: tests/test_nova_concurrency.c $(NOVA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(NOVA_SRCS) $< -o $@

test: all
	@echo "Running NOVA Queue Race Test..."
	@./tests/test_nova_queue_race
	@echo ""
	@echo "Running NOVA Concurrency Stress Test..."
	@./tests/test_nova_concurrency

clean:
	rm -f tests/test_nova_queue_race tests/test_nova_concurrency

.PHONY: all test clean
