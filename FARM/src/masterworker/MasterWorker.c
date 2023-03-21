#include <MasterWorker.h>

// thread signal handler
static void *SignalHandler(void *arg);
// cleanup handler
static void cleanup();
// helper printer
static void helper();

// numero workers
int NUM_THREAD;
// lunghezza coda
int Q_LENGHT;
// ritardo tra una richiesta e l'altra
int DELAY_TIME;
// PID del processo COLLECTOR
int collector_pid;
// coda concorrente dei task da elaborare
BQueue_t *coda;
// MUTEX per sig_interrupted: notifica al master di smettere di inviare richieste
pthread_mutex_t sig_interrupted_mtx = PTHREAD_MUTEX_INITIALIZER;
// MUTEX per coda: regola l'accesso alla coda concorrente dei task da elaborare
pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
// MUTEX per end_workers: usata dal master per notificare agli worker che è il momento di disconnettersi
pthread_mutex_t end_workers_mtx = PTHREAD_MUTEX_INITIALIZER;
// MUTEX per worker_counter: usata dal master per controllare quanti worker sono ancora online
pthread_mutex_t worker_counter_mtx = PTHREAD_MUTEX_INITIALIZER;
// CONDITION VARIABLE: usata per regolare l'accesso alla coda concorrente dei task da elaborare
pthread_cond_t queue_cond_empty = PTHREAD_COND_INITIALIZER;
// CONDITION VARIABLE: usata per regolare l'accesso alla coda concorrente dei task da elaborare
pthread_cond_t queue_cond_full = PTHREAD_COND_INITIALIZER;
// intero per il controllo dei segnali di interruzione
volatile int sig_interrupted;
// intero per il controllo della chiusura degli workers
volatile int end_workers;
// intero per il controllo del funzionamento degli workers
volatile int worker_counter;
// coda utilizzata dal master per l'invio dei task
Queue toSend;
// pipe utilizzata per la comunicazione intra-processi COLLECTOR-MASTERWORKER nella gestione del segnale SIGUSR1
int sh_pipe[2];
// TID del thread signal handler
pthread_t signal_handler_tid;

// MAIN
int main(int argc, char *argv[])
{
    // inizializzo alcune variabili
    DELAY_TIME = 0;
    Q_LENGHT = 8;
    NUM_THREAD = 4;
    coda = NULL;
    sig_interrupted = 0;
    end_workers = 0;
    worker_counter = 0;
    toSend = NULL;

    // controllo il numero degli argomenti
    if (argc < 2)
    {
        // in caso di errori stampo l'helper e chiudo
        helper();
        exit(EXIT_FAILURE);
    }

    // blocco nella maschera dei segnali i seguenti: verranno gestiti dal thread signal handler
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) == -1)
    {
        // in caso di errore devo necessariamente chiudere
        perror("in: pthread_sigmask");
        exit(EXIT_FAILURE); // errore fatale
    }

    // ignoro SIGPIPE per evitare una brusca chiusura del processo in caso di errori sui socket
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_handler = SIG_IGN;
    // system call sigaction: ignoro questo segnale
    if ((sigaction(SIGPIPE, &s, NULL)) == -1)
    {
        // in caso di errore devo neccesariamente chiudere
        perror("in: sigaction");
        exit(EXIT_FAILURE); // errore fatale
    }

    // creo la pipe per la comunicazione MASTERWORKER-COLLECTOR
    if (pipe(sh_pipe) == -1)
    {
        // in caso di errore devo neccesariamente chiudere
        perror("in: pipe");
        exit(EXIT_FAILURE); // errore fatale
    }

    // eseguo la fork: il processo e' ora duplicato
    collector_pid = fork();

    if (collector_pid == 0)
    {
        // sono il figlio, il processo COLLECTOR: avvio la routine
        collector();
        return 0;
    }

    // sono il padre, continuo con la gestione del processo MASTERWORKER

    // chiudo il lato di lettura della pipe
    close(sh_pipe[0]);

    // lancio il thread signal handler
    sh_args sh_arguments = {&mask, sh_pipe[1]};
    if (pthread_create(&signal_handler_tid, NULL, SignalHandler, &sh_arguments) != 0)
    {
        // in caso di errore devo neccesariamente chiudere
        perror("impossibile creare il thread signal handler");
        exit(EXIT_FAILURE); // errore fatale
    }

    // preparo gli argomenti da passare al thread master
    pthread_t master_tid;
    t_args args;
    // argc
    args.argc = argc;
    // argv
    args.argv = argv;
    // lancio il thread master
    if (pthread_create(&master_tid, NULL, Master, &args) != 0)
    {
        // in caso di errore devo neccesariamente chiudere
        perror("impossibile creare il thread Master");
        exit(EXIT_FAILURE); // errore fatale
    }
    // installo la funzione di cleanup che mi permette di terminare sempre in maniera sicura
    if (atexit(cleanup) != 0)
    {
        // in caso di errori proseguo lo stesso
        perror("in: atexit");
    }

    // mi metto in attesa della fine del thread master
    int error;
    if ((error = (pthread_join(master_tid, NULL))) != 0)
    {
        // in caso di errore setto errno e preoseguo lo stesso
        errno = error;
        perror("errore nella chiusura del thread Master");
    }
    // ritorno, attivando la funzione di cleanup
    return 0;
}

// funzione helper, stampa le opzioni della riga di comando
static void helper()
{
    fprintf(stdout, "Gli argomenti che opzionalmente possono essere passati al processo MasterWorker sono i seguenti:\n");
    fprintf(stdout, "-n <nthread> specifica il numero di thread Worker (valore di default 4)\n");
    fprintf(stdout, "-q <qlen> specifica la lunghezza della coda concorrente interna (valore di default 8)\n");
    fprintf(stdout, "-d <directory-name> specifica una directory in cui sono contenuti file binari ed eventualmente altre directory contenente file binari;\n");
    fprintf(stdout, "-t <delay> specifica un tempo in millisecondi che intercorre tra l’invio di due richieste successive (valore di default 0)\n");
    fflush(stdout);
}

// funzione di cleanup
static void cleanup()
{
    // cancello il thread signal handler che sarà fermo sul punto di cancellazione sigwait
    pthread_cancel(signal_handler_tid);
    // mi metto in attesa della fine del thread master
    int error;
    if ((error = (pthread_join(signal_handler_tid, NULL))) != 0)
    {
        // in caso di errore setto errno e preoseguo lo stesso
        errno = error;
        perror("errore nella chiusura del thread signal handler");
    }
    // chiudo il lato di scrittura della pipe
    close(sh_pipe[1]);
    // libero la coda interna del Master
    freeQueue(&toSend);
    // libero la coda concorrente dei task
    deleteBQueue(coda, NULL);
    // distruggo le mutex e le variabili di condizione
    pthread_cond_destroy(&queue_cond_empty);
    pthread_cond_destroy(&queue_cond_full);
    pthread_mutex_destroy(&sig_interrupted_mtx);
    pthread_mutex_destroy(&queue_mtx);
    pthread_mutex_destroy(&end_workers_mtx);
    pthread_mutex_destroy(&worker_counter_mtx);
    // cancello il socket file
    unlink(SOCKNAME);
    // in alcuni esiti negativi, ho bisogno di terminare brutalmente il processo COLLECTOR
    // nel migliore dei casi dove tutto va bene, questo comando non ha nessun esito
    kill(collector_pid, SIGKILL);
    // attendo la chiusura del processo COLLECTOR
    int status;
    if (waitpid(collector_pid, &status, 0) == -1)
    {
        // in caso di errore proseguo lo stesso
        perror("waitpid");
    }
    // controllo lo status di chiusura
    if (!WIFEXITED(status))
    {
        // se il collector è stato terminato per dei malfunzionamenti, stampo l'exit status
        fprintf(stdout, "Collector terminato con FALLIMENTO (%d)\n", WEXITSTATUS(status));
        fflush(stdout);
    }
}

// THREAD SIGNAL HANDLER
static void *SignalHandler(void *arg)
{
    // mi salvo il set di segnali da attendere
    sigset_t *set = ((sh_args *)arg)->set;
    // mi salvo il file descriptor di scrittura sulla pipe comunicante col collector
    int pipe = ((sh_args *)arg)->signal_pipe;
    // creo una variabile per il controllo del ciclo while del thread
    int out = 0;
    // ciclo fino a quando non ricevo un segnale di terminazione (o vengo cancellato)
    while (out != 1)
    {
        // chiamata bloccante che attende l'arrivo di uno dei segnali settati sulla maschera
        int sig;
        int r = sigwait(set, &sig);
        // se la chiamata non va a buon fine
        if (r != 0)
        {
            // setto errno e chiudo il thread
            errno = r;
            perror("in: sigwait (signal handler)");
            return NULL;
        }
        // switch sul tipo di segnale
        switch (sig)
        {
        // per tutti i segnali di terminazione
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        case SIGHUP:
            printf("SIGNAL HANDLER: ricevuto SIGINT\n\n");
            // modifico in maniera thread safe la variabile sig_interrupted
            // notifico così al master che non deve più inserire task nella coda concorrente
            LOCK(&sig_interrupted_mtx);
            sig_interrupted = 1;
            UNLOCK(&sig_interrupted_mtx);
            // cambio guardia del while
            out = 1;
            break;
        // caso SIGUSR1 per la stampa
        case SIGUSR1:
            printf("SIGNAL HANDLER: ricevuto SIGUSR1\n\n");
            // scrivo sulla pipe che ho ricevuto il segnale. il COLLECTOR stamperà la lista dei risultati
            if (writen(pipe, &sig, sizeof(int)) == -1)
            {
                // in caso di errore proseguo, il segnale verrà semplicemente perso
                perror("in: writen (signal handler)");
                break;
            }
            break;
        // caso di default
        default:
            printf("segnale non riconosciuto. questo non dovrebbe succedere...\n");
        }
    }
    // fine thread
    return NULL;
}
