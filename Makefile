COMMON_FLAGS =  -Wall -Wextra -Werror -g -O3 -mavx -Wno-error=unknown-pragmas -fopenmp
CFLAGS = $(COMMON_FLAGS) -std=c99 
CXXFLAGS = $(COMMON_FLAGS)
LDLIBS = -lm 
LDFLAGS = -fopenmp
export

COMMON = membership.o bucket_sort.o util.o linalg.o
SUBDIRS = tooling tests client_server

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS): $(COMMON)
	$(MAKE) -C $@

util.o: util.c util.h
bucket_sort.o: bucket_sort.c bucket_sort.h
membership.o: membership.c membership.h
linalg.o: linalg.c linalg.h

tests: tooling

clean: 
	rm -rf *.o
	cd tests && $(MAKE) clean
	cd tooling && $(MAKE) clean
	cd client_server && $(MAKE) clean