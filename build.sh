set -xe
mkdir -p ./build/
cc -std=c99 -Wall -I ./include/ -I ./vendor/ ./vendor/mpc.c ./src/builtin.c ./src/lval.c ./src/lenv.c ./src/main.c -ledit -lm -o ./build/noodle
