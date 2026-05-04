CC = gcc

CFLAGS = -ggdb -O0 -std=c99 -DUNITY_INCLUDE_DOUBLE -fstack-protector-all -Wall -Wextra -Werror=implicit-function-declaration
LDFLAGS = -O0

INCLUDES = -Iinclude -Iunity

TED_SRCS = src/main.c src/geo.c src/pm.c src/qry.c src/extensible_hash_file.c
TED_OBJS = $(TED_SRCS:.c=.o)

TEST_HASH_SRCS = tst/t_extensible_hash_file.c src/extensible_hash_file.c unity/unity.c
TEST_HASH_OBJS = $(TEST_HASH_SRCS:.c=.o)
TEST_HASH_BIN = tst/t_extensible_hash_file

TEST_GEO_SRCS = tst/t_geo.c src/geo.c src/extensible_hash_file.c unity/unity.c
TEST_GEO_OBJS = $(TEST_GEO_SRCS:.c=.o)
TEST_GEO_BIN = tst/t_geo

TEST_PM_SRCS = tst/t_pm.c src/pm.c src/geo.c src/extensible_hash_file.c unity/unity.c
TEST_PM_OBJS = $(TEST_PM_SRCS:.c=.o)
TEST_PM_BIN = tst/t_pm

TEST_QRY_SRCS = tst/t_qry.c src/qry.c src/pm.c src/geo.c src/extensible_hash_file.c unity/unity.c
TEST_QRY_OBJS = $(TEST_QRY_SRCS:.c=.o)
TEST_QRY_BIN = tst/t_qry

TEST_MORADOR_SRCS = tst/t_morador.c src/morador.c unity/unity.c
TEST_MORADOR_OBJS = $(TEST_MORADOR_SRCS:.c=.o)
TEST_MORADOR_BIN = tst/t_morador

TEST_QUADRA_SRCS = tst/t_quadra.c src/quadra.c unity/unity.c
TEST_QUADRA_OBJS = $(TEST_QUADRA_SRCS:.c=.o)
TEST_QUADRA_BIN = tst/t_quadra

TEST_BINS = $(TEST_HASH_BIN) $(TEST_GEO_BIN) $(TEST_PM_BIN) $(TEST_QRY_BIN) $(TEST_MORADOR_BIN) $(TEST_QUADRA_BIN)

.PHONY: all ted test_hash test_geo test_pm test_qry test_morador test_quadra tstall clean

all: ted

ted: $(TED_OBJS)
	$(CC) $(TED_OBJS) $(LDFLAGS) -o ted
	$(CC) $(TED_OBJS) $(LDFLAGS) -o src/ted

test_hash: $(TEST_HASH_BIN)

$(TEST_HASH_BIN): $(TEST_HASH_OBJS)
	$(CC) $(TEST_HASH_OBJS) $(LDFLAGS) -o $(TEST_HASH_BIN)

test_geo: $(TEST_GEO_BIN)

$(TEST_GEO_BIN): $(TEST_GEO_OBJS)
	$(CC) $(TEST_GEO_OBJS) $(LDFLAGS) -o $(TEST_GEO_BIN)

test_pm: $(TEST_PM_BIN)

$(TEST_PM_BIN): $(TEST_PM_OBJS)
	$(CC) $(TEST_PM_OBJS) $(LDFLAGS) -o $(TEST_PM_BIN)

test_qry: $(TEST_QRY_BIN)

$(TEST_QRY_BIN): $(TEST_QRY_OBJS)
	$(CC) $(TEST_QRY_OBJS) $(LDFLAGS) -o $(TEST_QRY_BIN)

test_morador: $(TEST_MORADOR_BIN)

$(TEST_MORADOR_BIN): $(TEST_MORADOR_OBJS)
	$(CC) $(TEST_MORADOR_OBJS) $(LDFLAGS) -o $(TEST_MORADOR_BIN)

test_quadra: $(TEST_QUADRA_BIN)

$(TEST_QUADRA_BIN): $(TEST_QUADRA_OBJS)
	$(CC) $(TEST_QUADRA_OBJS) $(LDFLAGS) -o $(TEST_QUADRA_BIN)

tstall: $(TEST_BINS)
	./$(TEST_HASH_BIN)
	./$(TEST_GEO_BIN)
	./$(TEST_PM_BIN)
	./$(TEST_QRY_BIN)
	./$(TEST_MORADOR_BIN)
	./$(TEST_QUADRA_BIN)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f src/*.o
	rm -f tst/*.o
	rm -f unity/*.o
	rm -f ted
	rm -f src/ted
	rm -f $(TEST_BINS)
	rm -f *.hf *.hfd *.svg
	rm -f tst_*.geo tst_*.pm tst_*.qry tst_*.txt tst_*.svg tst_*.hf
