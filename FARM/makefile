#compilatore
CC			=  gcc
#flags di compilazione
CFLAGS	    += -std=c99 -Wall -pedantic -g -D_DEFAULT_SOURCE
#cartella librerie
LIB_DIR 	= ./librerie
#cartella includes
INCL_DIR	= ./includes
#flag per l'inclusione 
INCLUDES	= -I ${INCL_DIR}
#libreria da utilizzare per la compilazione
LIBS        = -pthread
#per linkare librerie dinamiche
DYNLINK		= -Wl,-rpath=${LIB_DIR} -L ${LIB_DIR}

#OBJECTS DEPENDENCIES
#oggetti per FARM
FARM_OBJS =  objs/masterworker/Master.o objs/masterworker/MasterWorker.o objs/masterworker/Worker.o	objs/collector/Collector.o
#oggetti per GENERAFILE
GENERAFILE_OBJ = objs/generafile/generafile.o
#oggetti per LIBRERIA STRUTTURE
LIB_QUEUES_OBJS = objs/queues/boundedqueue.o objs/queues/orderedqueue.o
#BINARIES
FARM_BIN = farm
GENERAFILE_BIN = generafile
#TARGET MAKEFILE
TARGETS		= $(FARM_BIN) \
			  $(GENERAFILE_BIN)  
#PHONY CALLS
.PHONY: all cleanall cleantest test mytest
.SUFFIXES: .c .h
#default
all		: $(TARGETS)
#farm.exe
${FARM_BIN}: $(FARM_OBJS) $(LIB_DIR)/libQueues.so
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $(FARM_OBJS) $(LIBS) $(DYNLINK) -lQueues
#generafile.exe
${GENERAFILE_BIN}: $(GENERAFILE_OBJ) 
	$(CC) $(CFLAGS) -o $@ $(GENERAFILE_OBJ)
#libreria struttura dati
$(LIB_DIR)/libQueues.so: $(LIB_QUEUES_OBJS)
	$(CC) -shared -o $@ $^
#collector.o
objs/collector/Collector.o :src/collector/Collector.c $(INCL_DIR)/Collector.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -c -fPIC -o $@
#boundedqueue.o
objs/queues/boundedqueue.o: src/queues/boundedqueue.c $(INCL_DIR)/boundedqueue.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -c -fPIC -o $@
#generafile.o
objs/generafile/generafile.o: src/generafile/generafile.c
	$(CC) $(CFLAGS) $< -c -fPIC -o $@
#orderedqueue.o
objs/queues/orderedqueue.o: src/queues/orderedqueue.c $(INCL_DIR)/orderedqueue.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -c -fPIC -o $@
#worker.o
objs/masterworker/Worker.o: src/masterworker/Worker.c $(INCL_DIR)/Worker.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -c -fPIC -o $@
#master.o
objs/masterworker/Master.o: src/masterworker/Master.c $(INCL_DIR)/Master.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -c -fPIC -o $@	
#masterworker.o
objs/masterworker/MasterWorker.o: src/masterworker/MasterWorker.c $(INCL_DIR)/MasterWorker.h
	$(CC) $(CFLAGS) $(INCLUDES) $< -c -fPIC -o $@
#PHONY cleanall
cleanall:
		rm -f -r objs/*/*.o
		rm -f -r librerie/*
		rm -f -r collector
		rm -f -r farm
		rm -f -r generafile
		rm -f -r farm.sck
		rm -f -r testdir
		rm -f -r expected.txt
		rm -f -r file*
#PHONY cleantest
cleantest:
		rm -f -r testdir
		rm -f -r expected.txt
		rm -f -r file*
#PHONY test
test:
		chmod +x test.sh
		./test.sh
#PHONY mytest
mytest:
		chmod +x mytest.sh
		./mytest.sh