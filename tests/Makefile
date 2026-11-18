CC ?= clang
CFLAGS ?= -Wall -Wextra -Werror -O3 -pthread

ifneq ($(wildcard test_nova_queue_race.c),)
  ROOT = ..
  TESTS_DIR = .
else
  ROOT = .
  TESTS_DIR = tests
endif

INCLUDES = -I$(TESTS_DIR)/mock_mir -I$(ROOT)/patches/nova

NOVA_SRCS = $(ROOT)/patches/nova/nova_shadow_index.c \
            $(ROOT)/patches/nova/nova_coordinator.c \
            $(ROOT)/patches/nova/nova_cost.c \
            $(ROOT)/patches/nova/nova_dealer.c \
            $(ROOT)/patches/nova/nova_stealer.c \
            $(ROOT)/patches/nova/mir_arch_sim.c

TARGETS = $(TESTS_DIR)/test_nova_queue_race $(TESTS_DIR)/test_nova_concurrency

all: $(TARGETS)

$(TESTS_DIR)/test_nova_queue_race: $(TESTS_DIR)/test_nova_queue_race.c
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_DIR)/test_nova_concurrency: $(TESTS_DIR)/test_nova_concurrency.c $(NOVA_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(NOVA_SRCS) $< -o $@

test: all
	@echo "Running NOVA Queue Race Test..."
	@$(TESTS_DIR)/test_nova_queue_race
	@echo ""
	@echo "Running NOVA Concurrency Stress Test..."
	@$(TESTS_DIR)/test_nova_concurrency

clean:
	rm -f $(TARGETS)

.PHONY: all test clean

