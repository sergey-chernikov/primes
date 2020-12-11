#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "prime_db.h"


#define CHUNK		32*1024		/* Used for compressed pages */
#define WIDTH		100
#define	MAXPAGES	230

struct pdb_page_t
{
    PDBElem	first;
    PDBElem	last;
    size_t	nb_elems;
    int		compressed;
    off_t	offset;
};

struct pdb_page_meta
{
    off_t		offset;
    size_t		nb_pages;
    struct pdb_page_t	pages[MAXPAGES];
    PDBElem		first;
    PDBElem		last;
    off_t		next_meta;
};

typedef unsigned short	PDBDist;

#define NPRIMES1000	168
PDBElem first_primes[PAGESIZE] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61,
    67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179,
    181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307,
    311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439,
    443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587,
    593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727,
    733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829, 839, 853, 857, 859, 863, 877,
    881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997};

size_t n1primes = NPRIMES1000;
PDBElem max_elem = 0;
FILE *f = NULL;
volatile int stopped = 0;
struct pdb_page_meta cur_meta;


int pdb_read_page(const struct pdb_page_meta meta, size_t idx, PDBElem *page)
{
    if (! f)  return 0;
    PDBDist offsets[PAGESIZE - 1];
    flockfile(f);
    fseeko(f, meta.pages[idx].offset, SEEK_SET);
    size_t nb = fread(page, sizeof(PDBElem), 1, f);
    nb += fread(offsets, sizeof(PDBDist), meta.pages[idx].nb_elems - 1, f);
    funlockfile(f);
    if (nb < meta.pages[idx].nb_elems)  return 0;
    register int i;
    for (i = 1; i < meta.pages[idx].nb_elems; i++) {
	page[i] = page[i - 1] + offsets[i - 1];
    }
    return nb;
}

int pdb_read_meta(const off_t offset, struct pdb_page_meta *meta)
{
    if (! f)  return 0;
    flockfile(f);
    fseeko(f, offset, SEEK_SET);
    size_t nb = fread(meta, sizeof(struct pdb_page_meta), 1, f);
    funlockfile(f);
    if ((nb < 1) || !meta->nb_pages || (meta->nb_pages > MAXPAGES))  return 0;
    return 1;
}


int pdb_init(const char *fn)
{
    if (! fn)  return -1;
    f = fopen(fn, "rb+");
    if (! f)  f = fopen(fn, "wb+");
    if (! f)  return -2;
    if (!pdb_read_meta(0, &cur_meta) || (cur_meta.offset != 0)) {
	memset(&cur_meta, 0, sizeof(struct pdb_page_meta));
	printf("PrimeDB is not populated or corrupted\n");
    }
    else if (cur_meta.nb_pages > 0) {
	if ((n1primes = pdb_read_page(cur_meta, 0, first_primes)) > 0) {
	    printf("Preloaded %u first primes from %lu to %lu (max value in PrimeDB = %lu)\n",
		(unsigned int) n1primes, first_primes[0], first_primes[n1primes-1], pdb_get_max_elem());
	}
	else {
	    return -3;
	}
    }
    return 0;
}

void pdb_close()
{
    if (! f)  return;
    fclose(f);
    f = NULL;
}

void pdb_stop()
{
    stopped = 1;
}

PDBElem pdb_get_max_elem()
{
    if (max_elem)  return max_elem;
    max_elem = first_primes[NPRIMES1000 - 1];
    if (! f)  return max_elem;
    struct pdb_page_meta meta;
    if (! pdb_read_meta(0, &meta))  return max_elem;
    max_elem = meta.last;
    while (meta.next_meta) {
	if (! pdb_read_meta(meta.next_meta, &meta))  break;
	max_elem = meta.last;
    }
    return max_elem;
}

void pdb_write_page(const PDBElem *page, const size_t nb_elems)
{
    if (! f)  return;
    register int i;
    PDBDist offsets[PAGESIZE  - 1];
    for (i = 1; i < nb_elems; i++) {
	offsets[i - 1] = page[i] - page[i - 1];
    }
    fwrite(page, sizeof(PDBElem), 1, f);
    fwrite(offsets, sizeof(PDBDist), nb_elems - 1, f);
}

void pdb_append_page(const PDBElem start, const PDBElem end, const PDBElem *page, const size_t nb_elems)
{
    fseeko(f, 0, SEEK_END);
    off_t off_end = ftello(f);
    if (! off_end)  off_end = sizeof(struct pdb_page_meta);
    if (! cur_meta.first)  cur_meta.first = start;
    if (cur_meta.last < end)  cur_meta.last = end;
    cur_meta.pages[cur_meta.nb_pages].first = start;
    cur_meta.pages[cur_meta.nb_pages].last = end;
    cur_meta.pages[cur_meta.nb_pages].nb_elems = nb_elems;
    cur_meta.pages[cur_meta.nb_pages].compressed = 0;
    cur_meta.pages[cur_meta.nb_pages].offset = off_end;
    cur_meta.nb_pages++;

    fseeko(f, cur_meta.offset, SEEK_SET);
    fwrite(&cur_meta, sizeof(struct pdb_page_meta), 1, f);

    fseeko(f, off_end, SEEK_SET);
    pdb_write_page(page, nb_elems);

    if (cur_meta.nb_pages >= MAXPAGES) {
	fseeko(f, 0, SEEK_END);
	off_t next_offset = ftello(f);
	cur_meta.next_meta = next_offset;
	fseeko(f, cur_meta.offset, SEEK_SET);
	fwrite(&cur_meta, sizeof(struct pdb_page_meta), 1, f);
	memset(&cur_meta, 0, sizeof(struct pdb_page_meta));
	cur_meta.offset = next_offset;
	fseeko(f, next_offset, SEEK_SET);
	fwrite(&cur_meta, sizeof(struct pdb_page_meta), 1, f);
    }
    fflush(f);
}

int pdb_is_prime(const PDBElem n)
{
    PDBElem sqroot = sqrt(n) + 1;
    register int i;
    for (i = 0; i < n1primes; i++) {
	if (first_primes[i] > sqroot)  break;
	if ((n % first_primes[i]) == 0)  return 0;
    }

    PDBElem div = first_primes[n1primes-1] + 6;
    while (div < sqroot) {
	if ((n % div) == 0)  return 0;
	div += 2;
    }
    return 1;
}

void pdb_fill()
{
    if (! f)  return;
    PDBElem *page = (PDBElem *) malloc(PAGESIZE * sizeof(PDBElem));
    size_t nelems = 0;
    PDBElem start = 0, end = max_elem;
    if (cur_meta.nb_pages == 0) {
	memcpy(page, first_primes, n1primes * sizeof(PDBElem));
	nelems = n1primes;
	start = first_primes[0];
    }
    int nb_pages = 0;
    off_t offset = 0;
    while (pdb_read_meta(offset, &cur_meta)) {
	nb_pages += cur_meta.nb_pages;
	if (!cur_meta.next_meta)  break;
	offset = cur_meta.next_meta;
    }
    printf("Resume filling PrimeDB from %lu (%u pages with max size %d)\n", end, nb_pages, PAGESIZE);

    unsigned int step = PAGESIZE / WIDTH;
    stopped = 0;
    while (stopped == 0) {
	end += 2;
	if (pdb_is_prime(end) == 0)  continue;
	page[nelems++] = end;
	if (! start)  start = end;

	if ((nelems % step) == 0) {
	    printf(".");  fflush(stdout);
	}
	if (nelems >= PAGESIZE) {
	    printf("\nSaving page %d from %lu to %lu\n", (int)cur_meta.nb_pages+1, start, end);
	    pdb_append_page(start, end, page, nelems);
	    start = 0;
	    nelems = 0;
	    if (!cur_meta.nb_pages)  printf("Created new chunk of %d pages (%lu)\n", MAXPAGES, cur_meta.offset);
	}
    }
    pdb_close();
    free(page);
}

size_t pdb_get_page(const PDBElem start, PDBElem *page)
{
    if (start < first_primes[n1primes-1]) {
	memcpy(page, first_primes, n1primes*sizeof(PDBElem));
	return n1primes;
    }
    if (! f)  return 0;
    if (start >= max_elem)  return 0;

    struct pdb_page_meta meta;
    register size_t i;
    meta.next_meta = 0;
    do {
	if (! pdb_read_meta(meta.next_meta, &meta))  break;
	if (start > meta.last)  continue;
	for (i = 0; i < meta.nb_pages; i++) {
	    if (start < meta.pages[i].last)  break;
	}
	if (i >= meta.nb_pages)  return 0;
	return pdb_read_page(meta, i, page);
    } while (meta.next_meta > 0);
    return 0;
}

#define MAXDIST	2345
void pdb_stats(const char *opts)
{
    printf("Gathering PrimeDB statistics");
    struct timeval start, stop;
    struct pdb_page_meta meta;
    register size_t i, j;
    size_t  nb_elems = 0, nb_pages = 0, max_diff = 0, diff;
    PDBElem cur, prev = 0, md_prev=0, md_cur=0;
    unsigned int digits[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned int distances[MAXDIST];
    double revsum = 0;
    PDBElem *page = (PDBElem *) malloc(PAGESIZE * sizeof(PDBElem));
    memset(distances, 0, MAXDIST*sizeof(unsigned int));
    gettimeofday(&start, 0);

    meta.next_meta = 0;
    do {
	if (! pdb_read_meta(meta.next_meta, &meta))  break;
	printf(".");  fflush(stdout);
	nb_pages += meta.nb_pages;
	for (i = 0; i < meta.nb_pages; i++) {
	    if (! pdb_read_page(meta, i, page))  break;
	    nb_elems += meta.pages[i].nb_elems;
	    for (j = 0; j < meta.pages[i].nb_elems; j++) {
		cur = page[j];
		digits[0] = cur % 10;
		digits[digits[0]]++;
		diff = cur - prev;
		if (diff > max_diff) {
		    max_diff = cur - prev;
		    md_prev = prev,  md_cur = cur;
		}
		distances[diff]++;
		revsum += 1.0 / cur;
		prev = cur;
	    }
	}
    } while (meta.next_meta > 0);

    gettimeofday(&stop, 0);
    printf("\n");
    distances[2]--;
    free(page);
    printf("Total %u pages with %.1fM elements\n", (unsigned int) nb_pages, (double) nb_elems/1000000.0);
    printf("Max distance: %u (%lu, %lu) - %u time[s]\n", (unsigned int) max_diff, md_prev, md_cur,
	distances[max_diff]);
    if (strchr(opts, 'd') != NULL) {
	unsigned int nb_dists = 0;
	for (i = 2; i < MAXDIST; i++) {
	    if (distances[i] > 0)  nb_dists++;
	    if (distances[i] < (nb_elems / 10000))  continue;
	    printf("\tDistance %3u: %u times (%.3f%%)\n", (unsigned int) i, distances[i],
		(double) distances[i] * 100.0 / nb_elems);
	}
	printf("Total %u distances found\n", nb_dists);
    }
    printf("Average distance: %.3f\n\n", (double)(max_elem - 2) / nb_elems);

    printf("sum(1/p) = %f\n", revsum);
    for (i = 1; i < 10; i++) {
	if (digits[i] <= 1)  continue;
	printf("Primes with last digit %u: %u (%f%%)\n", (unsigned int) i,
	    (unsigned int) digits[i], (double) digits[i] * 100.0 / nb_elems);
    }
    double exec_tm = (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec)/1000000.0;
    printf("Execution time: %.3fs\n", exec_tm);
}
