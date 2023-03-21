#ifndef ORDEREDQUEUE_H_
#define ORDEREDQUEUE_H_
#include <util.h>

//struct nodo
typedef struct nodo {
	char path[256];
    long result;
	struct nodo *next;
}Nodo;

//puntatore a struct nodo = coda
typedef Nodo *Queue;

//inserimento
void insert(Queue *lPtr, long n, char*path, int ordered);
//free
void freeQueue(Queue *lPtr);
//stampa
void printQueue(Queue lPtr);
#endif