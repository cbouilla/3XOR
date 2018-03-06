tools: sort_hashes parse_shares check_shares bucketize split_shares match_solutions

sort_hashes.o: sort_hashes.c ../util.h hashes.h
sort_hashes: private CC = $(CXX)
sort_hashes: hashes.o sort.o ../util.o sort_hashes.o

parse_shares.o: parse_shares.c shares.h sha256.h hashes.h
parse_shares: ../util.o parse_shares.o shares.o hashes.o sha256.o

whatthefuck.o: whatthefuck.c shares.h hashes.h sha256.h
whatthefuck:   whatthefuck.o shares.o hashes.o sha256.o

check_shares.o: check_shares.c shares.h sort.h
check_shares:   check_shares.o shares.o sort.o sha256.o

split_shares.o: split_shares.c shares.h 
split_shares:   split_shares.o shares.o sha256.o

bucketize.o: bucketize.c hashes.h index.h
bucketize: ../bucket_sort.o hashes.o index.o bucketize.o

match_solutions.o: match_solutions.c sort.h shares.h ../client_server/client_server.h ../util.h
match_solutions: private CC = $(CXX)
match_solutions: ../util.o sort.o shares.o sha256.o match_solutions.o

sort.o: sort.cpp sort.h
tooling.o: tooling.c tooling.h
sha256.o: sha256.c sha256.h
shares.o: shares.c sha256.h shares.h
hashes.o: hashes.c hashes.h
index.o: index.c index.h

clean:
	rm -rf *.o sort_hashes parse_shares check_shares bucketize split_shares match_solutions