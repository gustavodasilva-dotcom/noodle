set -xe
mkdir -p ./build/
cc -std=c99 -Wall -I ./include/ ./src/mpc.c ./src/main.c -ledit -lm -o ./build/noodle
