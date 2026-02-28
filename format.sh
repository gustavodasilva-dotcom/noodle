set -xe
clang-format -i ./include/common.h ./include/builtin.h ./include/lenv.h ./include/lval.h ./include/parser.h ./src/builtin.c ./src/lval.c ./src/lenv.c ./src/main.c
