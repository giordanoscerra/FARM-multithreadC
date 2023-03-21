#include <Master.h>
#include <Worker.h>

// funzione per controllare il numero di workers online
static int workers_alive();
// funzione per esplorare le cartelle ricorsivamente
static void esplora_directory(char nomedir[]);
// funzione di util per la funzione esplora_directory
static int isdot(char dir[]);
// cleanup del master che gli permette in ogni caso di effettuare il join degli workers
static void master_cleanup(void *args);
// TRHEAD MASTER
void *Master(void *args)
{
	// recupero argc e argv dall'argomento
	t_args *a = (t_args *)args;
	int argc = (int)a->argc;
	char **argv = (char **)a->argv;
	// intero funzionale al getopt
	int opt;
	long n;
	// flags per controllare il corretto utilizzo della riga di comando
	int flag_n = 0, flag_q = 0, flag_t = 0, flag_d = 0;
	// array per salvare il nome della directory dell'opzione -d
	char source_dir[255];
	// variabile per notificare che non ci sono più workers online
	int no_workers = 0;
	// PARSING
	while ((opt = getopt(argc, argv, ":n:q:t:d:h")) != -1)
	{
		// switch sul carattere opt
		switch (opt)
		{
		// NUMERO DI WORKERS
		case 'n':
			// se avevo già usato l'opzione, ignoro il comando
			if (flag_n)
			{
				printf("il numero di thread e' gia' stato fissato.\n");
				break;
			}
			// controllo se è stato passato un numero > 0
			if (isNumber(optarg, &(n)) != 0 || n <= 0)
			{
				// altrimenti setto il numero di threads di default
				fprintf(stderr, "errore parsing -n: non e' stato inserito un numero di thread accettabile.\ndefault: 4\n");
				
			}
			else
			{
				// se ho passato un valore sensato, aggiorno la variabile NUMTHREADS
				NUM_THREAD = (int)n;
				// non accetterò altre opzioni -n
				flag_n = 1;
			}
			break;
		// LUNGHEZZA CODA CONCORRENTE DEI TASK
		case 'q':
			// se avevo già usato l'opzione, ignoro il comando
			if (flag_q)
			{
				printf("la dimensione della coda e' gia' stata fissata..\n");
				break;
			}
			// controllo se è stato passato un numero > 0
			if (isNumber(optarg, &(n)) != 0 || n <= 0)
			{
				fprintf(stderr, "errore parsing -q: non e' stata inserita una dimensione accettabile.\ndefault: 8\n");
				
			}
			else
			{
				// se ho passato un valore sensato, aggiorno la variabile QLENGHT
				Q_LENGHT = (int)n;
				// non accetterò altre opzioni -q
				flag_q = 1;
			}
			break;

		case 't':
			// se avevo già usato l'opzione, ignoro il comando
			if (flag_t)
			{
				printf("il tempo di delay e' gia' stato fissato.\n");
				break;
			}
			// controllo se è stato passato un numero >= 0
			if (isNumber(optarg, &(n)) != 0 || n < 0)
			{
				fprintf(stderr, "errore parsing -t: non e' stata inserito un numero accettabile.\ndefault: 0\n");
				
			}
			else
			{
				// se ho passato un valore sensato, aggiorno la variabile DELAYTIME
				DELAY_TIME = (int)(n / 1000);
				// non accetterò altre opzioni -t
				flag_t = 1;
			}
			break;

		case 'd':
			// se avevo già usato l'opzione, ignoro il comando
			if (flag_d)
			{
				printf("la sorgente dei file e' gia' stata specificata.\n");
				break;
			}
			else
			{
				// copio il path passato nella variabile source_dir che verrà controllata dopo
				strncpy(source_dir, optarg, 255 - 1);
				// non accetterò altre opzioni -d
				flag_d = 1;
			}
			break;
		// caso in cui l'opzione è giusta ma manca l'argomento
		case ':':
			printf("manca un argomento: consultare sezione help (-h)\n");
			pthread_exit(NULL); // chiusura soft
			break;
		// caso di comando non riconosciuto
		default:
			printf("comando non riconosciuto: consultare sezione help (-h)\n");
			pthread_exit(NULL); // chiusura soft
		}
	}
	// inizializzo la coda concorrente dei task
	if ((coda = initBQueue(Q_LENGHT)) == NULL)
	{
		// in caso di errore chiudo il thread MASTER
		perror("impossibile inizializzare la coda");
		pthread_exit(NULL); // chiusura soft
	}
	// creo un array di thread id: questo sarà il mio threadpool
	pthread_t tids[NUM_THREAD];
	// lancio NUMTHREAD threads worker
	for (int i = 0; i < NUM_THREAD; i++)
	{
		if (pthread_create(&tids[i], NULL, Worker, NULL) != 0)
		{
			// in caso di errore devo necessariamente chiudere il thread MASTER
			perror("impossibile aggiungere worker");
			pthread_exit(NULL); // chiusura soft
		}
	}
	// installo una routine di cleanup che mi permetterà di joinare gli workers in caso di errore e non
	pthread_cleanup_push(master_cleanup, tids);
	// esploro la directory passata con -d e mi salvo tutti i suoi file in una lista non ordinata
	if (flag_d)
	{
		esplora_directory(source_dir);
	}

	// controllo se i file passati come lista nella riga di comando siano regolari e li inserisco nella lista non ordinata dei file da inviare agli workers
	struct stat statbuf;
	// notused
	long bluff = -2;
	// scorro tutte le stringhe non-options ignorate da getopt in argv
	for (int index = optind; index < argc; index++)
	{
		// eseguo la stat e controllo se si tratta di un file regolare
		if ((stat(argv[index], &statbuf) == 0) && S_ISREG(statbuf.st_mode))
		{
			// se è un file regolare, alloco una stringa per salvarmi il path
			char *filename = malloc(256 * sizeof(char));
			// se non c'è memoria continuo al prossimo file
			if (filename == NULL)
			{
				perror("malloc");
				continue;
			}
			// mi trascrivo la stringa da argv a una mia copia
			strncpy(filename, argv[index], 255);
			// la inserisco nella lista non ordinata dei file da inviare agli workers
			insert(&toSend, bluff, filename, 0);
			// libero la memoria allocata
			SAFE_FREE(filename);
		}
		// in caso di errori nella stat, non mi fermo
		else
		{
			perror("stat");
			printf("%s non e' regolare !\n", argv[index]);
		}
	}
	// PROTOCOLLO PRODUTTORE CONSUMATORE
	// ciclo finché non ho scorso tutta la lista dei file da inviare o ricevo un segnale di terminazione
	while (toSend != NULL && check_termination(&sig_interrupted, &sig_interrupted_mtx) == 0)
	{
		// sleep tra una richiesta e l'altra
		sleep(DELAY_TIME);
		// lock della coda concorrente dei task
		LOCK(&queue_mtx);
		//devo mettermi in attesa e rilasciare la lock se:
		//-la coda è piena
		//-non ho ancora ricevuto un segnale di terminazione
		//-ci sono ancora workers attivi
		int out = 0;
		while (coda->qlen == coda->qsize && (out=check_termination(&sig_interrupted, &sig_interrupted_mtx)) == 0 && workers_alive() != -1)
			WAIT(&queue_cond_full, &queue_mtx);
		//se non ci sono più workers online
		if (workers_alive() == -1)
		{
			//non occorre fare una signal poiché non ho inserito nulla nella coda
			//unlock della coda
			UNLOCK(&queue_mtx);
			printf("\n\nNO WORKERS LEFT ONLINE!!!\n\n");
			//segnalo che non occorrerà nemmeno fare una broadcast
			no_workers = 1;
			//esco dal while
			break;
		}
		//se ho ricevuto un segnale di terminazione
		if (out == 1)
		{
			//non occorre fare una signal poiché non ho inserito nulla nella coda
			//unlock della coda
			UNLOCK(&queue_mtx);
			//esco dal while
			break;
		}
		//push del filename nella coda concorrente dei task 
		if (push(coda, toSend->path) == -1)
		{
			//in caso di errore continuo
			perror("push");
		}
		//segnalo ad un worker che ho inserito un elemento nella coda
		SIGNAL(&queue_cond_empty);
		//unlock della coda concorrente
		UNLOCK(&queue_mtx);
		//passo al prossimo file da inviare
		toSend = toSend->next;
	}

	//se ci sono ancora workers online dovrò avvisarli di terminare
	if (no_workers == 0)
	{
		//lock della coda concorrente
		LOCK(&queue_mtx);
		//devo mettermi in attesa e rilasciare la lock se:
		//-gli workers non hanno ancora finito di elaborare tutte le richieste
		while (coda->qlen != 0)
			WAIT(&queue_cond_full, &queue_mtx);
		//setto a uno in maniera thread safe la variabile di terminazione degli workers
		LOCK(&end_workers_mtx);
		end_workers = 1;
		UNLOCK(&end_workers_mtx);
		//sveglio tutti gli workers in attesa
		BCAST(&queue_cond_empty);
		//rilascio la lock
		UNLOCK(&queue_mtx);
	}
	//segnalo che bisognerà sempre eseguire la routine di cleanup
	pthread_cleanup_pop(1);
	//ritorno
	return 0;
}
//funzione di util alla funzione esplora_directory
static int isdot(char dir[])
{
	int l = strlen(dir);

	if ((l > 0 && dir[l - 1] == '.'))
		return 1;
	return 0;
}
//funzione per esplorare ricorsivamente una cartella, inserisce i filename nella lista non ordinata dei filename da inviare agli workers
static void esplora_directory(char nomedir[])
{
	
	struct stat statbuf;
	//controllo se si tratta di una directory
	if (stat(nomedir, &statbuf) == -1)
	{
		//se non la è, ritorno
		perror("stat");
		return;
	}
	//creo un puntatore a cartella
	DIR *dir;
	//apro la cartella
	if ((dir = opendir(nomedir)) == NULL)
	{	
		//in caso di errore ritorno
		perror("opendir");
		return;
	}
	else
	{	
		//se è una cartella inizio la lettura ricorsiva
		struct dirent *file;
		//ciclo fino a quando errno non è settato a 0 e la lettura ha successo
		while ((errno = 0, file = readdir(dir)) != NULL)
		{
			struct stat statbuf;
			//costruisco il path relativo della cartella
			char pathname[256];
			strncpy(pathname, nomedir, 255 - 1);
			strncat(pathname, "/", 255 - 1);
			strncat(pathname, file->d_name, 255 - 1);
			//stat sul pathname
			if (stat(pathname, &statbuf) == -1)
			{	
				//in caso di errore ritorno
				perror("stat");
				return;
			}
			//se il file è di nuovo una cartella
			if (S_ISDIR(statbuf.st_mode))
			{
				//e non è la cartella parent
				if (!isdot(pathname))
					//chiamata ricorsiva
					esplora_directory(pathname);
			}
			//se si tratta di un file regolare 
			else if(S_ISREG(statbuf.st_mode))
			{
				//alloco la memoria per una stringa
				char *filename = malloc(256 * sizeof(char));
				//se non c'è più memoria continuo con il prossimo file
				if (filename == NULL)
				{
					perror("malloc");
					continue;
				}
				//creo una copia del filename
				strncpy(filename, pathname, 256);
				//notused
				long bluff = -1;
				//inserisco il filename nella lista non ordinata di filename da inviare agli workers
				insert(&toSend, bluff, filename, 0);
				//libero la memoria
				SAFE_FREE(filename);
			}
		}
		//se c'è stato un errore nella lettura della cartella lo segnalo e chiudo l'fd cartella
		if (errno != 0)
			perror("readdir");
		closedir(dir);
	}
}
//funzione utile sia a Master che a Worker: lettura threadsafe di una variabile
int check_termination(volatile int *flag, pthread_mutex_t *mtx)
{
	//acquisisco la lock sulla mutex
	LOCK(mtx);
	//se la variabile è settata
	if (*flag == 1)
	{
		//unlock 
		UNLOCK(mtx);
		//ritorno 1
		return 1;
	}
	//altrimenti unlock e ritorno 0
	UNLOCK(mtx);
	return 0;
}
//funzione per controllare se ci sono workers ancora online
static int workers_alive()
{
	//lock sulla mutex
	LOCK(&worker_counter_mtx);
	//se non ci sono workers online
	if (worker_counter == 0)
	{
		//unlock e ritorno -1
		UNLOCK(&worker_counter_mtx);
		return -1;
	}
	//altrimenti unlock e ritorno 1
	UNLOCK(&worker_counter_mtx);
	return 1;
}
//cleanup del master: eseguita sempre, effettua join del threadpool
static void master_cleanup(void *args)
{
	//mi salvo l'argomento 
	pthread_t *tids = (pthread_t *)args;
	//effettuo la join di tutti gli workers
	int error;
	for (int i = 0; i < NUM_THREAD; i++)
	{
		if ((error = (pthread_join(tids[i], NULL))) != 0)
		{	
			//in caso di errore setto errno ma non mi fermo
			errno = error;
			perror("errore nella chiusura di uno worker");
		}
	}
}