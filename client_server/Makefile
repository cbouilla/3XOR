all: server quad_client quadratic gjoux # gjoux_client

quadratic: ../membership.o ../util.o ../tooling/hashes.o ../tooling/index.o task_quad.o common.o quadratic.o

gjoux: LDLIBS += -lm4ri
gjoux: ../util.o ../tooling/hashes.o ../linalg.o ../bucket_sort.o ../membership.o task_gjoux.o common.o gjoux.o

server: LDLIBS += -lzmq
server: server.o

quad_client: LDLIBS += -lzmq -lm4ri
quad_client: ../membership.o ../util.o ../tooling/hashes.o ../tooling/index.o task_quad.o common.o quad_client.o

quad_task.o: quad_task.c tasks.h ../membership.h ../util.h
task_gjoux.o: gjoux.c tasks.h ../util.h ../bucket_sort.h ../linalg.h ../membership.h
quadratic.o: quadratic.c tasks.h ../tooling/hashes.h ../tooling/index.h
gjoux.o: gjoux.c tasks.h ../util.h ../tooling/hashes.h

clean: 
	rm -rf *.o quadratic gjoux gjoux_client server quad_client
