#!/bin/sh

CC=gcc

rm -f primes

#$CC -I/usr/local/include -L/usr/local/lib -O3 -march=native -mmmx -ftree-vectorize \
$CC -m64 -O3 -march=native -mmmx -ftree-vectorize -Wall \
    prime.c prime_db.c -o primes -pthread -lpthread -lgmp -lm
