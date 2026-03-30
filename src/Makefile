CC := gcc

CSTD := -std=c99
CSEC := -fstack-protector-all
CWARN := -Wall -Wextra -Werror=implicit-function-declaration
CDBG := -ggdb -O0

CFLAGS := $(CSTD) $(CSEC) $(CWARN) $(CDBG)
LDFLAGS := $(CSEC)

UNITY_DIR := Unity
HASH_DIR := extensible_hash_file

INCLUDES := -I$(UNITY_DIR) -I$(HASH_DIR)

PROJ_NAME := ted

TED_SRCS ?=
TED_OBJS := $(TED_SRCS:.c=.o)

HASH_MODULE_SRC := $(HASH_DIR)/extensible_hash_file.c
HASH_MODULE_HDR := $(HASH_DIR)/extensible_hash_file.h
HASH_MODULE_OBJ := $(HASH_DIR)/extensible_hash_file.o

HASH_TEST_SRC := $(HASH_DIR)/t_extensible_hash_file.c
HASH_TEST_BIN := $(HASH_DIR)/t_extensible_hash_file
UNITY_SRC := $(UNITY_DIR)/unity.c
UNITY_HDRS := $(UNITY_DIR)/unity.h $(UNITY_DIR)/unity_internals.h
UNITY_OBJ := $(UNITY_DIR)/unity.o

.PHONY: all ted clean tests tstall test_hash_extensible run_test_hash_extensible help

all: ted

# Produz o executável do projeto principal.
ted: $(TED_OBJS)
ifeq ($(strip $(TED_OBJS)),)
	@echo "ERRO: ainda nao ha fontes do programa principal definidos em TED_SRCS."
	@echo "Edite o Makefile e preencha TED_SRCS quando o arquivo main/modulos do TED existirem."
	@exit 1
else
	$(CC) $(TED_OBJS) $(LDFLAGS) -o $(PROJ_NAME)
endif

# Testes unitários

tests: test_hash_extensible

tstall: test_hash_extensible
	@echo "Todos os testes unitarios configurados foram executados."

test_hash_extensible: $(HASH_TEST_BIN)
	./$(HASH_TEST_BIN)

run_test_hash_extensible: test_hash_extensible

$(HASH_TEST_BIN): $(HASH_TEST_SRC) $(HASH_MODULE_OBJ) $(UNITY_OBJ) $(HASH_MODULE_HDR) $(UNITY_HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) $(HASH_TEST_SRC) $(HASH_MODULE_OBJ) $(UNITY_OBJ) -o $@

$(HASH_MODULE_OBJ): $(HASH_MODULE_SRC) $(HASH_MODULE_HDR)
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

$(UNITY_OBJ): $(UNITY_SRC) $(UNITY_HDRS)
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

# -----------------------------------------------------------------------------
# Limpeza
# -----------------------------------------------------------------------------
clean:
	rm -f $(TED_OBJS)
	rm -f $(HASH_MODULE_OBJ) $(UNITY_OBJ)
	rm -f $(HASH_TEST_BIN)
	rm -f $(PROJ_NAME)
	rm -f $(HASH_DIR)/*.dat $(HASH_DIR)/*.hf $(HASH_DIR)/*.hfd
	rm -f *.dat *.hf *.hfd *.svg *.txt

# -----------------------------------------------------------------------------
# Ajuda rápida
# -----------------------------------------------------------------------------
help:
	@echo "Alvos disponiveis:"
	@echo "  make ted                  - compila o executavel principal (quando TED_SRCS estiver preenchido)"
	@echo "  make test_hash_extensible - compila e executa os testes do modulo hash extensivel"
	@echo "  make tstall               - compila e executa todos os testes unitarios configurados"
	@echo "  make clean                - remove objetos, executaveis e arquivos temporarios"
