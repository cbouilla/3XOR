#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "util.h"


double wtime()
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return (double)ts.tv_sec + ts.tv_usec / 1E6;
}

uint64_t rdtsc()
{
	uint64_t low, high;
	__asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
	return (high << 32) | low;
}

int tid()
{
#ifdef _OPENMP
	return omp_get_thread_num();
#else
	return 0;
#endif
}


int num_threads()
{
#ifdef _OPENMP
	return omp_get_num_threads();
#else
	return 1;
#endif
}


int max_threads()
{
#ifdef _OPENMP
	return omp_get_max_threads();
#else
	return 1;
#endif
}

int num_places()
{
#ifdef _OPENMP
	return omp_get_num_places();
#else
	return 0;
#endif	
}


int place_num()
{
#ifdef _OPENMP
	return omp_get_place_num();
#else
	return -1;
#endif	
}


int parallel_level()
{
#ifdef _OPENMP
	return omp_get_level();
#else
	return 0;
#endif	
}