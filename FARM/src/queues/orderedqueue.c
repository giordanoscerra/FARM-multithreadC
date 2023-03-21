#include <orderedqueue.h>

// inserimento
void insert(Queue *lPtr, long n, char *path, int ordered)
{

	// allocazione memoria
	Queue newPtr = malloc(sizeof(Nodo));

	// se è andato tutto bene
	if (newPtr != NULL)
	{
		//e desidero un inserimento ordinato
		if (ordered == 1)
		{ // ciclo fino alla posizione corretta
			while (*lPtr != NULL && (*lPtr)->result < n)
			{

				//next
				lPtr = &((*lPtr)->next);
			}
		}

		//aggiungo un nuovo nodo
		strncpy(newPtr->path, path, 255);
		newPtr->result = n;
		newPtr->next = *lPtr;
		*lPtr = newPtr;
		
	}
	// se non c'è più memoria
	else
	{
		
		printf("Errore: Memoria esaurita.\n");
	}
}



//liberare memoria heap
void freeQueue(Queue *lPtr)
{

	// puntatore temporaneo
	Queue tempPtr = NULL;

	// scorro tutta la lista
	while (*lPtr != NULL)
	{
		
		tempPtr = *lPtr;
		*lPtr = (*lPtr)->next;
		//free 
		SAFE_FREE(tempPtr);
	}
}


//stampa
void printQueue(Queue lPtr)
{

	//scorro la lista
	while (lPtr != NULL)
	{
		// stampa
		printf("%ld %s\n", lPtr->result, lPtr->path);
		//next
		lPtr = lPtr->next;
	}
}