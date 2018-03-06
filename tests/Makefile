test:  doctor_hashes

doctor_hashes.o: doctor_hashes.c ../tooling/tooling.h tests.h
doctor_hashes: private CC = $(CXX)
doctor_hashes: ../tooling/sort.o chacha.o rand.o doctor_hashes.o

chacha.o: chacha.s tests.h

clean:
	rm -rf *.o doctor_hashes