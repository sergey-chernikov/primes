#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <gmp.h>
#include "prime_db.h"


#define NCPU		4
#define WIDTH		100

PDBElem* primes = NULL;
size_t nprimes = 0;
PDBElem tbl_max_val = 0;
PDBElem* pages = NULL;
unsigned long long count = 0;
volatile int nrunning = 0;
pthread_mutex_t lckStdout;
int redirected = 0;

struct thr_data {
   mpz_t	num;
   PDBElem* page;
   PDBElem	tbl_start, tbl_end;
   mpz_t	div_start, div_end;
   int		result;
   int		progress;
};


void erase(const size_t n)
{
   register unsigned int i;
   pthread_mutex_lock(&lckStdout);
   for (i = 0; i < n; i++) {
      printf("\b \b");
   }
   fflush(stdout);
   pthread_mutex_unlock(&lckStdout);
}

void* thr_isPrime(void* data)
{
   struct thr_data* vData = (struct thr_data*)data;
   if (!redirected && vData->progress) {
      pthread_mutex_lock(&lckStdout);
      printf("?");
      fflush(stdout);
      pthread_mutex_unlock(&lckStdout);
   }

   int found = 0;
   register size_t i;
   size_t nb_elems;
   while ((nb_elems = pdb_get_page(vData->tbl_start, vData->page)) > 0) {
      for (i = 0; i < nb_elems; i++) {
         if (vData->page[i] < vData->tbl_start)  continue;
         if (vData->page[i] >= vData->tbl_end)  break;
         if ((nrunning <= 0) || mpz_divisible_ui_p(vData->num, vData->page[i])) {
            found = 1;
            vData->result = 0;
            break;
         }
      }
      if (found)  break;
      vData->tbl_start = vData->page[nb_elems - 1] + 6;
      if (vData->tbl_start >= vData->tbl_end)  break;
   }

   if (found) {
      nrunning = 0;
   } else {
      mpz_t div;
      mpz_init_set(div, vData->div_start);
      while (mpz_cmp(div, vData->div_end) <= 0) {
         if (nrunning <= 0) {
            vData->result = 0;
            break;
         }

         if (mpz_divisible_p(vData->num, div)) {
            vData->result = 0;
            nrunning = 0;
            break;
         }
         mpz_add_ui(div, div, 2);
      }
      mpz_clear(div);
   }
   nrunning--;
   if (!redirected && vData->progress)  erase(1);
   return NULL;
}

int isPrime(const mpz_t num, int progress)
{
   mpz_t sqrt;
   mpz_init(sqrt);
   mpz_sqrt(sqrt, num);
   mpz_add_ui(sqrt, sqrt, 1);
   int result = 0, found = 0;
   register size_t i;
   for (i = 0; i < nprimes; i++) {
      if (mpz_cmp_ui(sqrt, primes[i]) <= 0) {
         result = 1;
         found = 1;
         break;
      }
      if (mpz_divisible_ui_p(num, primes[i])) {
         found = 1;
         break;
      }
   }
   if (found) {
      mpz_clear(sqrt);
      return result;
   }

   PDBElem start = primes[nprimes - 1] + 6;
   PDBElem tbl_step = tbl_max_val;
   if (mpz_cmp_ui(sqrt, tbl_step) < 0)  tbl_step = mpz_get_ui(sqrt);
   tbl_step = (tbl_step - start) / NCPU;
   mpz_t step, div;
   mpz_init_set_ui(div, tbl_max_val + 6);
   mpz_init_set(step, sqrt);
   mpz_sub(step, step, div);
   mpz_div_ui(step, step, NCPU);
   struct thr_data data[NCPU];
   pthread_t threads[NCPU];
   nrunning = NCPU;
   result = 0;
   for (i = 0; i < NCPU; i++) {
      mpz_t div_start, div_end;
      mpz_init_set(div_start, step);  mpz_init_set(div_end, step);
      mpz_mul_ui(div_start, div_start, i);
      mpz_mul_ui(div_end, div_end, i + 1);
      data[i].page = pages + i * PAGESIZE;
      data[i].result = 1;
      data[i].progress = progress;
      data[i].tbl_start = start + i * tbl_step;
      data[i].tbl_end = start + (i + 1) * tbl_step;
      mpz_init_set(data[i].num, num);
      mpz_init_set(data[i].div_start, div);
      mpz_add(data[i].div_start, data[i].div_start, div_start);
      if (mpz_divisible_ui_p(data[i].div_start, 2)) {
         mpz_sub_ui(data[i].div_start, data[i].div_start, 1);
      }
      if ((i + 1) == NCPU) {
         mpz_init_set(data[i].div_end, sqrt);
      } else {
         mpz_init_set(data[i].div_end, div);
         mpz_add(data[i].div_end, data[i].div_end, div_end);
         mpz_sub_ui(data[i].div_end, data[i].div_end, 1);
      }
      if (pthread_create(&(threads[i]), NULL, thr_isPrime, &(data[i]))) {
         fprintf(stderr, "Error creating thread %d\n", (int)i);
         exit(2);
      }
      mpz_clears(div_start, div_end, NULL);
   }
   for (i = 0; i < NCPU; i++) {
      if (pthread_join(threads[i], NULL)) {
         fprintf(stderr, "Error joining thread %d\n", (int)i);
         exit(2);
      }
      mpz_clears(data[i].num, data[i].div_start, data[i].div_end, NULL);
      result += data[i].result;
   }
   mpz_clears(step, div, NULL);
   return (result == NCPU) ? 1 : 0;
}

int isMersenne(const mpz_t num)
{
   mpz_t mnum;
   mpz_init_set(mnum, num);
   mpz_add_ui(mnum, mnum, 1);
   mpz_t result;
   mpz_init(result);
   mpz_and(result, num, mnum);
   int rc = 0;
   if (mpz_cmp_ui(result, 0) == 0)  rc = 1;
   mpz_clears(mnum, result, NULL);
   return rc;
}

void checkPrimality(const mpz_t num)
{
   mpz_out_str(stdout, 10, num);
   if (!isPrime(num, 1)) {
      printf(" is not a prime\n");
      return;
   }
   if (isMersenne(num))  printf(" is a Mersenne prime\n");
   else  printf(" is an ordinal prime number (%d)\n", mpz_probab_prime_p(num, 23));
}

void findWagstaffPrimesQ(unsigned long long num)
{
   printf("Finding all Wagstaff primes for base %llu\n", num);
   if ((num % 2) == 1) {
      printf("Odd base %llu can't form a Wagstaff number\n", num);
      return;
   }
   mpz_t wNum;
   mpz_init(wNum);
   printf("q:");
   for (unsigned long int q = 3; q <= ULONG_MAX; q += 2) {
//      mpz_pow_ui(wNum, wBase, q);
      mpz_ui_pow_ui(wNum, num, q);
      mpz_add_ui(wNum, wNum, 1);
      if (!mpz_divisible_ui_p(wNum, num + 1)) {
         continue;
      }
      printf(" %lu", q); fflush(stdout);
      mpz_divexact_ui(wNum, wNum, num + 1);
      if (isPrime(wNum, 0)) {
         printf("\nQ(%llu, %lu) is a prime\n", num, q);
         if (isMersenne(wNum)) {
            mpz_out_str(stdout, 10, wNum);
            printf(" is also a Mersenne prime\n");
         }
         count++;
      }
   }
}

void findWagstaffPrimesB(unsigned long long num)
{
   printf("Finding all Wagstaff primes for exponent %llu\n", num);
   if ((num % 2) == 0) {
      printf("Even exponent %llu can't form a Wagstaff number\n", num);
      return;
   }
   mpz_t wNum;
   mpz_init(wNum);
   printf("b:");
   for (unsigned long int b = 2; b <= ULONG_MAX; b += 2) {
      //      mpz_pow_ui(wNum, wBase, q);
      mpz_ui_pow_ui(wNum, b, num);
      mpz_add_ui(wNum, wNum, 1);
      if (!mpz_divisible_ui_p(wNum, b + 1)) {
         continue;
      }
      printf(" %lu", b); fflush(stdout);
      mpz_divexact_ui(wNum, wNum, b + 1);
      if (isPrime(wNum, 0)) {
         printf("\nQ(%lu, %llu) is a prime\n", b, num);
         if (isMersenne(wNum)) {
            mpz_out_str(stdout, 10, wNum);
            printf(" is also a Mersenne prime\n");
         }
         count++;
      }
   }
}

void factorize(const mpz_t num)
{
   mpz_t n;
   mpz_init_set(n, num);
   mpz_out_str(stdout, 10, n);
   if (isPrime(num, 1)) {
      printf(" is not factorizable as it is a prime number\n");
      return;
   }
   printf(" = ");
   fflush(stdout);

   PDBElem* page = (PDBElem*)malloc(PAGESIZE * sizeof(PDBElem));

   unsigned long cnt = 0, iters = 0;
   register int i;
   mpz_t div, limit, start;
   mpz_init_set_ui(start, tbl_max_val);
   while (mpz_cmp_ui(n, 1) > 0) {
      if (iters++ > 100) {
         printf("\nToo much iterations - giving up\n");
         break;
      }
      unsigned int uidiv = 0;
      PDBElem uistart = 0;
      size_t nb_elems;
      while ((nb_elems = pdb_get_page(uistart, page)) > 0) {
         if (mpz_cmp_ui(n, uistart) < 0)  break;
         for (i = 0; i < nb_elems; i++) {
            if (mpz_cmp_ui(n, page[i]) < 0)  break;
            if (mpz_divisible_ui_p(n, page[i])) {
               if (cnt)  printf(" * ");
               printf("%lu", page[i]);
               mpz_divexact_ui(n, n, page[i]);
               cnt++;  uidiv++;
               fflush(stdout);
            }
         }
         uistart = page[nb_elems - 1] + 6;
      }
      if (mpz_cmp_ui(n, 1) == 0)  break;
      if (isPrime(n, 0)) {
         printf(" * ");  mpz_out_str(stdout, 10, n);
         cnt++;
         break;
      }
      if (uidiv)  continue;

      mpz_init_set(div, start);
      mpz_init(limit);
      mpz_sqrt(limit, n);
      while (mpz_cmp(div, limit) <= 0) {
         mpz_add_ui(div, div, 2);
         if (mpz_divisible_p(n, div)) {
            if (cnt)  printf(" * ");
            mpz_divexact(n, n, div);
            mpz_out_str(stdout, 10, div);
            mpz_set(start, div);
            cnt++;
            fflush(stdout);
            break;
         }
      }
      mpz_clears(div, limit, NULL);
   }
   mpz_clear(start);
   free(page);
   printf("\n%lu divisors\n", cnt);
}


void sigterm(int signal)
{
   pdb_stop();
   printf(" \nFound %llu primes                                ", count);
   nrunning = 0;
   sleep(1);
   pthread_mutex_destroy(&lckStdout);
   printf("\n");
   pdb_close();
   exit(0);
}

void siginfo(int signal)
{
   printf(" \nFound %llu primes\n", count);
}

void init_signals()
{
   struct sigaction sigact_term;
   sigact_term.sa_handler = sigterm;
   sigemptyset(&sigact_term.sa_mask);
   sigact_term.sa_flags = 0;
   sigaction(SIGINT, &sigact_term, (struct sigaction*)NULL);
#ifdef SIGINFO
   struct sigaction sigact_info;
   memcpy(&sigact_info, &sigact_term, sizeof(struct sigaction));
   sigact_info.sa_handler = siginfo;
   sigaction(SIGINFO, &sigact_info, (struct sigaction*)NULL);
#endif //SIGINFO
}

void usage()
{
   printf("Prime numbers research tool\n"
      "\t-W <b>\tsearch Wagstaff prime of base b\n");
}

int main(int argc, const char** argv)
{
   init_signals();
   if (pthread_mutex_init(&lckStdout, NULL)) {
      fprintf(stderr, "Failed to init stdout mutex\n");
      return 2;
   }
   redirected = !isatty(fileno(stdout));

   if (pdb_init("primes.bin") < 0) {
      fprintf(stderr, "Failed to initialize PrimeDB!\n");
      return 3;
   }

   pages = (PDBElem*)malloc(NCPU * PAGESIZE * sizeof(PDBElem));
   primes = (PDBElem*)malloc(PAGESIZE * sizeof(PDBElem));
   nprimes = pdb_get_page(0, primes);
   tbl_max_val = pdb_get_max_elem();

   int increment = 1;
   unsigned long long maxcount = 0;
   mpz_t num, end;
   mpz_init_set_ui(num, 4294967295);
   mpz_init_set_ui(end, 0);
   if (argc == 1) {
      printf("Filling PrimeDB with new factors/divisors\n");
      pdb_fill();
      return 0;
   }
   if (argc > 1) {
      if (argv[1][0] == '%') {
         pdb_stats(argv[1] + 1);
         return 0;
      } else if (argv[1][0] == '=') {
         mpz_set_str(num, argv[1] + 1, 10);
         checkPrimality(num);
         exit(0);
      } else if (argv[1][0] == '-') {
         switch (argv[1][1]) {
         case 'h':
         case 'H':
            usage();
            exit(0);
            break;
         case 'w':
            if (argc < 2) {
               usage();
               exit(1);
            }
            findWagstaffPrimesB(strtoull((const char*)argv[2], 0, 10));
            exit(0);
            break;
         case 'W':
            if (argc < 2) {
               usage();
               exit(1);
            }
            findWagstaffPrimesQ(strtoull((const char*)argv[2], 0, 10));
            exit(0);
            break;
         default:
            printf("unknown option %c\n", argv[1][1]);
            break;
         }
      } else {
         mpz_set_str(num, argv[1], 10);
      }
   }
   if (argc > 2) {
      if (argv[2][0] == '-') {
         maxcount = atoi(argv[2] + 1);
         if (!maxcount)  maxcount = 1;
         increment = -1;
      } else if (argv[2][0] == '+') {
         maxcount = atoi(argv[2] + 1);
         if (!maxcount)  maxcount = 1;
      } else if (argv[2][0] == '=') {
         checkPrimality(num);
         exit(0);
      } else if (argv[2][0] == '/') {
         mpz_t div;
         mpz_init(div);
         mpz_sqrt(div, num);
         factorize(num);
         exit(0);
      } else {
         mpz_set_str(end, argv[2], 10);
      }
   }
   if (!maxcount)  printf("Finding all primes starting from ");
   else  printf("Finding %llu prime[s] starting from ", maxcount);
   mpz_out_str(stdout, 10, num);
   if (mpz_cmp_ui(end, 0) > 0) {
      printf(" to ");
      mpz_out_str(stdout, 10, end);
   }
   printf("\n");

   if (increment > 0)  mpz_sub_ui(num, num, 1);
   else  mpz_add_ui(num, num, 1);
   char snum[1024];
   size_t len = 0;

   while ((mpz_cmp_ui(end, 0) == 0) || (mpz_cmp(num, end) <= 0)) {
      if (increment > 0)  mpz_add_ui(num, num, increment);
      else  mpz_sub_ui(num, num, -increment);

      mpz_get_str(snum, 10, num);
      size_t sznum = strlen(snum);
      if (!redirected) {
         printf("%s", snum);
         fflush(stdout);
      }

      if (isPrime(num, 1) == 0) {
         if (!redirected)  erase(sznum);
         continue;
      }
      if (redirected)  printf("%s", snum);

      len += sznum;
      if (isMersenne(num) == 1) {
         printf("(M)");
         len += 3;
      }
      if (!redirected)  fflush(stdout);

      count++;
      if ((len + sznum + 1) >= WIDTH) {
         printf(" \n");
         len = 0;
      } else {
         printf(" ");
         len++;
      }
      if (maxcount && (count >= maxcount))  break;
   }
   printf("\nFound %llu primes\n", count);

   return 0;
}
