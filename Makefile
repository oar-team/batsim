CC=clang
CFLAGS=-Wall -Wextra
LDFLAGS=-lsimgrid -lm -ljansson

all: batsim batexec

batsim: batsim.o job.o export.o machines.o utils.o
	$(CC) -o $@ $^ $(LDFLAGS)

batexec: batexec.o job.o export.o machines.o utils.o
	$(CC) -o $@ $^ $(LDFLAGS)

batsim.o: batsim.c batsim.h job.h export.h machines.h utils.h
	$(CC) -o $@ -c $< $(CFLAGS)

batexec.o: batexec.c job.h utils.h
	$(CC) -o $@ -c $< $(CFLAGS)

job.o: job.c job.h utils.h
	$(CC) -o $@ -c $< $(CFLAGS)

export.o: export.c export.h job.h utils.h
	$(CC) -o $@ -c $< $(CFLAGS)

machines.o: machines.c machines.h
	$(CC) -o $@ -c $< $(CFLAGS)

utils.o: utils.c utils.h job.h
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -f *.o

mrproper: clean
	rm -f batsim batexec

distclean: mrproper
