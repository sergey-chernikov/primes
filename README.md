# primes
Prime numbers generation and checking tool

This tool was written in pure C and is designed as a testbed for GMP (GNU MathPoint) library. It can do the following:
* Generate a desired number of primes less than or greater than an arbitrary number
* Factorize a given number down to divisors if it's not prime
* Explore prime numbers for fun
* Occupy one or more CPUs with some work

Input and output numbers can have any size (far exceed 2^64 and even 2^128), thanks to GMP library.

 <h2>Command-line help</h2>
 When started without arguments the tool starts or resumes the prime number generation into primes.bin file located in the current directory. PrimeDB (primes.bin format) is a compact format for holding primes in the unsigned long form (up to 2^64 - 1 on 64-bit architectures). It consumes less space on disk than Sieve of Eratosthenes (8 bits in one char).
 
<code>   primes number =</code><br/>
Checks whether number is an ordinal, Mersenne (2^p-1) prime or not a prime at all:
<pre>./primes 2305843009213693951 =
Preloaded 1000000 first primes from 2 to 15485863 (max value in PrimeDB = 28024330787)
2305843009213693951 is a Mersenne prime</pre>

<code>  primes number /</code><br/>
Factorizes number into divisors:
<pre>./primes 111111111111111111111111111111111111111111111111111111111111111111111111111111111111 /
Preloaded 1000000 first primes from 2 to 15485863 (max value in PrimeDB = 28024330787)
111111111111111111111111111111111111111111111111111111111111111111111111111111111111 = 3 * 7 * 11 * 13 * 29 * 37 * 43 * 101 * 127 * 239 * 281 * 1933 * 2689 * 4649 * 9901 * 226549 * 459691 * 909091 * 10838689 * 121499449 * 7 * 4458192223320340849
22 divisors</pre>

<code>  primes number +/-[num]</code><br/>
Searches for num (or 1 if num not specified) primes forth and back from the number:
<pre>time ./primes 590295810358705651713 +23
Preloaded 1000000 first primes from 2 to 15485863 (max value in PrimeDB = 28048384121)
Finding 23 prime[s] starting from 590295810358705651713
590295810358705651741 590295810358705651817 590295810358705651829 590295810358705651951
590295810358705651961 590295810358705651997 590295810358705652021 590295810358705652153
590295810358705652167 590295810358705652273 590295810358705652281 590295810358705652287
590295810358705652381 590295810358705652429 590295810358705652563 590295810358705652627
590295810358705652629 590295810358705652681 590295810358705652797 590295810358705652809
590295810358705652813 590295810358705652837 590295810358705652891
Found 23 primes
549.435u 27.418s 2:32.84 377.4% 25+34715k 0+0io 0pf+0w</pre>

<code>  primes number1 number2</code><br/>
Searches for primes between number1 and number2:
<pre>./primes 9999900 10000000
Preloaded 1000000 first primes from 2 to 15485863 (max value in PrimeDB = 28096512887)
Finding all primes starting from 9999900 to 10000000
9999901 9999907 9999929 9999931 9999937 9999943 9999971 9999973 9999991
Found 9 primes</pre>

<code>  primes %</code><br/>
Prints some statistics on PrimeDB:
<pre>./primes %
Preloaded 1000000 first primes from 2 to 15485863 (max value in PrimeDB = 28096512887)
Gathering PrimeDB statistics......
Total 1221 pages with 1221.0M elements
Max distance: 456 (25056082087, 25056082543) - 1 time[s]
Average distance: 23.011

sum(1/p) = 3.442003
Primes with last digit 1: 305249344 (24.999946%)
Primes with last digit 3: 305253162 (25.000259%)
Primes with last digit 7: 305250469 (25.000038%)
Primes with last digit 9: 305247023 (24.999756%)
Execution time: 17.552s</pre>
If "d" is added after "%" then a more detailed distance statistics is printed.

<h2>Building and running</h2>
This program was ever built and test on FreeBSD 9-STABLE/10-STABLE 64-bit and GMP 5.1.3. Build and run it on your own risk.

To increase/decrease thread concurrency one may alter the "NCPU" define in prime.c. Building is performed by running the shell script.
