#include <algorithm>
#include <cstdint>
//#include <tbb/parallel_sort.h>

//extern "C" void sort_list_parallel(uint64_t *L, size_t from, size_t to);
extern "C" void sort_list_sequential(uint64_t *L, int64_t from, int64_t to);

/*void sort_list_parallel(uint64_t *L, size_t from, size_t to)
{
	tbb::parallel_sort(&L[from], &L[to]);
}*/


void sort_list_sequential(uint64_t *L,  int64_t from, int64_t to)
{
	std::sort(&L[from], &L[to]);
}