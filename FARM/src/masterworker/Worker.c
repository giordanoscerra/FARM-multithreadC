#include <Worker.h>

// funzione per connettersi al collector
static int connect_to_collector(int *sockfd);
// funzione per disconnettersi al collector
static void disconnect();

// THREAD WORKER
void *Worker(void *arg)
{
	// modifico in maniera thread safe il counter degli worker attivi
	LOCK(&worker_counter_mtx);
	worker_counter++;
	UNLOCK(&worker_counter_mtx);
	// creo il file descriptor per il socket di connessione
	int sockfd = 0;
	// provo a connettermi al collector che starà in ascolto
	if (connect_to_collector(&sockfd) == -1)
	{
		// se c'è un problema diminuisco il counter degli worker attivi
		perror("worker: impossibile connettersi");
		LOCK(&worker_counter_mtx);
		worker_counter--;
		UNLOCK(&worker_counter_mtx);
		// sono costretto ad un'uscita brutale per gestire alcuni casi di errore
		exit(EXIT_FAILURE); //chiusura hard
	}

	// da ora in poi il thread worker si disconnetterà sempre in chiusura
	pthread_cleanup_push(disconnect, &sockfd);
	//filepath
	char *file = NULL;
	int out = 0;
	// implemento un protocollo produttore consumatore
	while (1)
	{
		// acquisisco la lock sulla coda concorrente dei task
		LOCK(&queue_mtx);
		//devo mettermi in attesa e rilasciare la lock se:
		//-la coda è vuota
		//-non ho ancora ricevuto nessun segnale di terminazione
		while (coda->qlen == 0 && (out = check_termination(&end_workers, &end_workers_mtx)) == 0)
		{
			// mi metto in attesa di un segnale sulla condition variable
			WAIT(&queue_cond_empty, &queue_mtx);
		}
		// se sono uscito dal ciclo perché ho ricevuto un segnale di terminazione, esco dal while
		if (out == 1)
		{
			// unlock coda ed esco dal while
			UNLOCK(&queue_mtx);
			break;
		}
		// se è stato inserito un file nella coda, lo estraggo
		file = (char *)pop(coda);
		// segnalo al master che ho estratto un file dalla coda
		SIGNAL(&queue_cond_full);
		// unlock della coda
		UNLOCK(&queue_mtx);

		// TEST: lo script compulsive.sh crea una situazione di errore in cui tutti gli workers cadono
		////////////////////////////////////////////////////
		if (strcmp(file, "miniTest/file1") == 0) ///////////
		{										 ///////////
			SAFE_FREE(file);					 ///////////
			break;								 ///////////
		}										 ///////////
		if (strcmp(file, "miniTest/file2") == 0) ///////////
		{										 ///////////
			SAFE_FREE(file);					 ///////////
			break;								 ///////////
		}										 ///////////
		////////////////////////////////////////////////////

		//creo un puntatore a file
		FILE *fin;
		//provo ad aprire il file estratto in modalità di lettura bytes
		if ((fin = fopen(file, "rb")) == NULL)
		{	
			//se c'è un errore proseguo
			perror("worker: open file");
		}
		else
		{
			//se sono riuscito ad aprire il file
			long i = 0;
			long result = 0;
			long readNumber;
			//preparo un buffer di 8 posizioni per leggere un long
			unsigned char buffer[8];
			//ciclo di lettura da file nel buffer
			while (fread(buffer, sizeof(buffer), 1, fin) == 1)
			{
				//eseguo il calcolo 
				readNumber = *((long *)buffer);
				result += i * readNumber;
				i++;
			}
			//se c'è stato un errore nella lettura da file
			if (ferror(fin) != 0)
			{
				//chiudo il fd del file
				printf("c'è un errore nella lettura.\n");
				fclose(fin);
				//libero la stringa
				SAFE_FREE(file);
				//continuo con il prossimo file
				continue;
			}
			//calcolo la lunghezza del filepath da inviare
			int len = strlen(file) + 1;
			//invio la lunghezza del filepath al collector
			if (writen(sockfd, &len, sizeof(int)) == -1)
			{
				//in caso di errore mi disconnetto
				perror("worker write: lenght");
				fclose(fin);
				SAFE_FREE(file);
				break;
			}
			//invio il filepath al collector
			if (writen(sockfd, file, len * sizeof(char)) == -1)
			{
				//in caso di errore mi disconnetto
				perror("worker write: pathfile");
				fclose(fin);
				SAFE_FREE(file);
				break;
			}
			//invio il risultato del calcolo al collector
			if (writen(sockfd, &result, sizeof(long)) == -1)
			{
				//in caso di errore mi disconnetto
				perror("worker write: result");
				fclose(fin);
				SAFE_FREE(file);
				break;
			}
			//mi preparo a ricevere la risposta
			int response;
			//leggo la risposta del collector
			if (readn(sockfd, &response, sizeof(int)) <= 0)
			{
				//in caso di errore mi disconnetto
				perror("worker read: OK not received");
				fclose(fin);
				SAFE_FREE(file);
				break;
			}
			//se la risposta non è positiva segnalo l'errore
			if (response != 1)
			{
				//mi disconnetto
				errno = EPROTO;
				perror("worker read: broken protocol");
				fclose(fin);
				SAFE_FREE(file);
				break;
			}
			//chiudo i descrittori e libero la memoria
			fclose(fin);
			SAFE_FREE(file);
		}
	}	
	//dovrò eseguire il cleanup di disconnessione ogni volta che il worker ritorna
	pthread_cleanup_pop(1);
	return 0;
}

//funzione per connettersi al collector
static int connect_to_collector(int *sockfd)
{
	//ogni worker è un client
	struct sockaddr_un serv_addr;
	//creo il socket
	*sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (*sockfd == -1)
	{	
		//in caso di errore ritorno errore
		perror("worker socket : creation");
		return -1;
	}
	//preparo le informazioni per connettermi
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, "farm.sck", strlen("farm.sck") + 1);
	//creo uno pseudo-timer che mi permetterà di non ciclare all'infinito nel while sotto
	int times = 0;
	//ciclo finché non mi connetto al collector o ho raggiunto il limite massimo di tentativi
	while ((connect(*sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) && times < 1000)
	{
		//ritento se il socket non esiste oppure esiste ma il collector non sta ancora ascoltando
		if (errno == ENOENT || errno == ECONNREFUSED)
		{
			//dormo per 500 millisecondi
			sleep(0.005);
			//incremento il contatore dei tentativi
			times++;
		}
		//in tutti gli altri casi ritorno errore
		else
		{
			return -1;
		}
	}
	//se sono uscito dal ciclo perché ho sforato i tentativi, ritorno errore
	if (times == 1000)
	{
		return -1;
	}
	//mi sono connesso
	return 1;
}

//funzione di cleanup del worker per disconnettersi dal collector
static void disconnect(void *socket)
{

	//decremento in maniera thread safe il counter degli worker online
	LOCK(&worker_counter_mtx);
	worker_counter--;
	UNLOCK(&worker_counter_mtx);
	//mi preparo a disconnettermi
	int *sockfd = (int *)socket;
	int close_connection = -1;
	int r;
	int response;
	//scrivo sul socket il valore speciale per la disconnessione
	if ((r = writen(*sockfd, &close_connection, sizeof(int))) == -1)
	{
		//in caso di errore, stampo un errore e ritorno 
		perror("worker write: close connection");
		close(*sockfd);
		return;
	}
	//leggo la risposta del collector
	if ((r = readn(*sockfd, &response, sizeof(int))) <= 0)
	{
		//in caso di errore, stampo un errore e ritorno 
		perror("worker read: OK close connection");
		close(*sockfd);
		return;
	}
	//se la risposta non è positiva
	if (response != 1)
	{
		//setto errno, stampo errore e ritorno
		errno = EPROTO;
		perror("worker read: broken protocol");
		close(*sockfd);
		return;
	}
	//chiudo il socket del worker
	close(*sockfd);
}