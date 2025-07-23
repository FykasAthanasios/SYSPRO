CC = gcc
CFLAGS = -Wall -Wextra -g

final: worker fss_manager fss_console

fss_console: fss_console.o implementation_fss_console.o
	$(CC) $(CFLAGS) fss_console.o implementation_fss_console.o -o fss_console

fss_manager: fss_manager.o implementation_fss_manager.o
	$(CC) $(CFLAGS) fss_manager.o implementation_fss_manager.o -o fss_manager

worker: worker.o implementation_worker.o
	$(CC) $(CFLAGS) worker.o implementation_worker.o -o worker

fss_console.o: fss_console.c fss_console.h
	$(CC) $(CFLAGS) -c fss_console.c

implementation_fss_console.o: implementation_fss_console.c fss_console.h
	$(CC) $(CFLAGS) -c implementation_fss_console.c

fss_manager.o: fss_manager.c fss_manager.h
	$(CC) $(CFLAGS) -c fss_manager.c

implementation_fss_manager.o: implementation_fss_manager.c fss_manager.h
	$(CC) $(CFLAGS) -c implementation_fss_manager.c

worker.o: worker.c
	$(CC) $(CFLAGS) -c worker.c

implementation_worker.o: implementation_worker.c
	$(CC) $(CFLAGS) -c implementation_worker.c

clean:
	rm -f *.o fss_manager worker fss_console
