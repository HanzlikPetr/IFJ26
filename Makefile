#Petr Hanzlik (xhanzlp00)
#IFJ26

#Nastavení kompilátoru a jeho parametrů
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -MMD -MP -g -Isrc
VALGRIND = valgrind --quiet --leak-check=full --track-origins=yes --error-exitcode=1

#Nastavení zdrojových souborů a cílového souboru
SRCS = $(wildcard ./src/*.c)
SRCS_OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard ./src/*.h ./tests/utest/*.h)
TESTS = $(wildcard ./tests/utest/*.c)
TESTS_OBJS = $(TESTS:.c=.o)
TEST_TARGETS = $(patsubst ./tests/utest/test_%.c,utest_%,$(TESTS))
TEST_SRCS_OBJS = $(filter-out ./src/compiler.o,$(SRCS_OBJS))

TARGET = ifj26
TEST_LDLIBS = -lcmocka

#Seznam všech závislostí, které budou generovány při kompilaci
ALL_DEPS = $(SRCS_OBJS:.o=.d) $(TESTS_OBJS:.o=.d)

#Vychozi cil je build (spousti se pri pouhem prikazu make)
.DEFAULT_GOAL := build

.PHONY: all build clean format test test-systest

#---------------------------------------------------
# Pravidla pro kompilaci hlavniho programu
#---------------------------------------------------
build: $(TARGET)

$(TARGET): $(SRCS_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

#---------------------------------------------------
# Pravidla pro kompilaci testů
#---------------------------------------------------
test: $(TEST_TARGETS)
	@echo ""
	@echo "Running tests without Valgrind..."
	@echo ""
	@for test in $(TEST_TARGETS); do \
		echo "Running test: $$test";\
		./$$test || exit 1;\
	done

	@echo ""
	@echo "Running tests with Valgrind..."
	@echo ""

	@for test in $(TEST_TARGETS); do \
		echo "Running test: $$test";\
		$(VALGRIND) ./$$test || exit 1;\
	done

# test_psa mockuje scanner_next_token a scanner_peek_next_token náhradou
# symbolů při linkování (GNU ld --wrap), proto má vlastní linkovací pravidlo
utest_psa: ./tests/utest/test_psa.o $(TEST_SRCS_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(TEST_LDLIBS) -Wl,--wrap=scanner_next_token -Wl,--wrap=scanner_peek_next_token

$(filter-out utest_psa,$(TEST_TARGETS)): utest_%: ./tests/utest/test_%.o $(TEST_SRCS_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(TEST_LDLIBS)

test-format:
	@echo ""
	@echo "Running format check with clang-format, if there are any errors, please run 'make format' to fix them. If there are no errors, the output will be empty."
	@echo ""
	@ find . -type f \( -name "*.c" -o -name "*.h" \) \
            -exec clang-format --style=file --dry-run --Werror {} +

test-systest: build
	@echo ""
	@echo "Running system tests..."
	@echo ""
	@./tests/system/run_systests.sh || exit 1

#---------------------------------------------------
# Pravidlo pro formátování kódu
#---------------------------------------------------
format:
	clang-format --style=file -i $(SRCS) $(HEADERS) $(TESTS)

#---------------------------------------------------
# Pravidlo pro vytvoření archivu
#---------------------------------------------------
zip: clean
	zip -r xhanzlp00.zip src Makefile rozdeleni dokumentace.pdf rozsireni


#---------------------------------------------------
# Pravidlo pro čištění repozitáře
#---------------------------------------------------
clean:
	rm -f  $(SRCS_OBJS) $(TESTS_OBJS) $(TEST_TARGETS) $(ALL_DEPS) $(TARGET)

-include $(ALL_DEPS)
