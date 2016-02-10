
#define PAGESIZE	1000000
typedef unsigned long	PDBElem;

int pdb_init(const char *fn);
PDBElem pdb_get_max_elem();
void pdb_fill();
void pdb_close();
void pdb_stop();
int pdb_stopped();
size_t pdb_get_page(const PDBElem start, PDBElem *page);
void pdb_stats(const char *opts);
