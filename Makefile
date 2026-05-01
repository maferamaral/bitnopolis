CC = gcc

CFLAGS = -ggdb -O0 -std=c99 -DUNITY_INCLUDE_DOUBLE -fstack-protector-all -Wall -Wextra -Werror=implicit-function-declaration
LDFLAGS = -O0

INCLUDES = -Iinclude -Iunity

TED_SRCS = src/main.c src/geo.c src/pm.c src/qry.c src/extensible_hash_file.c
TED_OBJS = $(TED_SRCS:.c=.o)

TEST_HASH_SRCS = tst/t_extensible_hash_file.c src/extensible_hash_file.c unity/unity.c
TEST_HASH_OBJS = $(TEST_HASH_SRCS:.c=.o)
TEST_HASH_BIN = tst/t_extensible_hash_file

.PHONY: all ted test_hash tstall clean

all: ted

ted: $(TED_OBJS)
	$(CC) $(TED_OBJS) $(LDFLAGS) -o src/ted

test_hash: $(TEST_HASH_BIN)

$(TEST_HASH_BIN): $(TEST_HASH_OBJS)
	$(CC) $(TEST_HASH_OBJS) $(LDFLAGS) -o $(TEST_HASH_BIN)

tstall: test_hash
	./$(TEST_HASH_BIN)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f src/*.o
	rm -f tst/*.o
	rm -f unity/*.o
	rm -f src/ted
	rm -f $(TEST_HASH_BIN)
	rm -f saida/*.svg saida/*.txt saida/*.hf saida/*.hfd
	rm -f *.hf *.hfd *.svg
