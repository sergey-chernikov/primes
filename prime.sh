#!/bin/sh

CC=gcc

rm -f primes

$CC -I/usr/local/include -L/usr/local/lib -O3 -march=native -mmmx -ftree-vectorize  \
    -lm -lpthread -lgmp -Wall prime.c prime_db.c -g -o primes
