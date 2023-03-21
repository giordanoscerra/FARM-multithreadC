#include <Collector.h>
//contatore di client attivi
int active_clients = 0;
//funzione per chiudere una connessione con un client
static void close_connection(int fd, fd_set *set, int *max_fd);
//PROCESSO COLLECTOR
void collector()
{   
    //chiudo l'end di scrittura della pipe in comunicazione con il masterworker
    close(sh_pipe[1]);
    //creo una coda ordinata per l'output
    Queue output_queue = NULL;
    //mi preparo alla connessione
    int listen_fd, client_fd;
    // indice dell'fd di valore più alto da ascoltare
    int max_fd = 0; 
    struct sockaddr_un serv_addr;
    //distruggo l'eventuale socket file 
    (void)unlink(SOCKNAME);
    memset(&serv_addr, '0', sizeof(serv_addr));
    strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME) + 1);
    serv_addr.sun_family = AF_UNIX;
    //creo il socket
    if ((listen_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        //in caso di errore devo necessariamente chiudere tutto
        perror("collector: socket");
        exit(EXIT_FAILURE); // errore fatale
    }
    //effettuo la bind del socket
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        //in caso di errore devo necessariamente chiudere tutto
        perror("collector: bind");
        exit(EXIT_FAILURE); // errore fatale
    }
    //mi metto in ascolto sul listen socket
    if (listen(listen_fd, SOMAXCONN) == -1)
    {
        //in caso di errore devo necessariamente chiudere tutto
        perror("collector: listen");
        exit(EXIT_FAILURE); // errore fatale
    }
    //creo i set di ascolto per il selector
    fd_set tmpset, set;
    // inizializzo il set di fd che la select deve "ascoltare"
    FD_ZERO(&set);
    //listen socket
    FD_SET(listen_fd, &set);
    // pipe per la comunicazione con signal handler thread
    FD_SET(sh_pipe[0], &set);
    // setto max_fd
    max_fd = MAX(listen_fd, sh_pipe[0]);
    int res;
    //variabile per la chiusura del collector
    int close_end = 0;
    //ciclo while
    while (close_end != 1)
    {
        //aggiorno il set dei fd da ascoltare
        tmpset = set;
        //preparo un timeout di 10 secondi
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        //select: quali fd sono pronti in lettura?
        if ((res = select(max_fd + 1, &tmpset, NULL, NULL, &timeout)) < 0)
        {
            //in caso di errore devo chiudere il collector
            perror("collector: select");
            close_end = 1; // errore fatale
            continue; //uscirò dal while
        }
        //se è scaduto il timer, devo chiudere il collector
        //è improbabile che il collector stia fermo per così tanto tempo.
        if (res == 0)
        { 
          printf("COLLECTOR: TIME OUT.\n");
          fflush(stdout);
          close_end = 1;
          continue;
        }
        //scorro i file descriptors pronti in lettura
        for (int i = 0; i <= max_fd; i++)
        {
            //fd pronto
            if (FD_ISSET(i, &tmpset))
            {
                //nuova connessione !
                if (i == listen_fd)
                {
                    // listen_fd è pronto ad accettare una nuova connessione
                    if ((client_fd = accept(listen_fd, NULL, NULL)) == -1)
                    {
                        perror("collector: accept");
                        continue; //errore non fatale
                    }
                    //incremento il counter dei client attivi
                    active_clients++;
                    //aggiungo l'fd nuovo al set degli fd da ascoltare
                    FD_SET(client_fd, &set);
                    //aggiorno eventualmente il max_fd
                    if (client_fd > max_fd)
                        max_fd = client_fd;
                }
                //segnale SIGUSR1 dal MASTERWORKER
                else if (i == sh_pipe[0])
                {   
                    //leggo il segnale
                    int sig;
                    if (readn(i, &sig, sizeof(int)) <= 0)
                    {
                        //se la pipe si è chiusa, la scarto
                        close_connection(i, &set, &max_fd);
                        continue; //continuo
                    }
                    //se ho ricevuto SIGUSR1, stampo la lista ordinata finora
                    if (sig == SIGUSR1)
                    {
                        printf("COLLECTOR: ricevuto SIGUSR1.\n");
                        fflush(stdout);
                        printf("-----------------------\n");
                        printQueue(output_queue);
                        printf("-----------------------\n");
                    }
                    else {
                        //in tutti gli altri casi setto errno e scarto la pipe
                        errno = EPIPE;
                        perror("collector pipe");
                        close_connection(i, &set, &max_fd);
                        continue; //continuo
                    }
                }
                //devo leggere da un client già connesso
                else
                {
                    //mi preparo alla lettura
                    int len = 0;
                    int readn_res;
                    long result;
                    char *str;
                    int n;
                    int ok = 1;
                    //leggo la lunghezza
                    if ((readn_res = readn(i, &len, sizeof(int))) <= 0)
                    {
                        //se c'è un errore chiudo la connessione e scarto il fd
                        perror("collector read: message lenght");
                        close_connection(i, &set, &max_fd);
                        continue; //continuo
                    }
                    //ho ricevuto il valore speciale per chiudere la connessione
                    if (len == -1)
                    {
                        //rispondo che ho ricevuto correttamente la richiesta
                        if ((n = writen(i, &ok, sizeof(int))) == -1)
                        {
                            //se c'è un errore continuo lo stesso
                            perror("collector write: OK close connection"); // continuo
                        }
                        //chiudo la connessione e scarto l'fd
                        close_connection(i, &set, &max_fd);
                        //se non ci sono più client attivi, posso chiudere il collector
                        if (active_clients == 0) close_end = 1;

                    }

                    //se ho ricevuto qualcosa di sensato
                    else if (len > 0)
                    {
                        //alloco memoria per lettura filename
                        if ((str = calloc(len, sizeof(char))) == NULL)
                        {
                            //se c'è un errore chiudo la connessione libero memoria e scarto il fd
                            perror("collector: calloc");
                            close_connection(i, &set, &max_fd);
                            SAFE_FREE(str);
                            continue;
                        }
                        //leggo il filename
                        if ((readn_res = readn(i, str, len * sizeof(char))) <= 0)
                        {
                            //se c'è un errore chiudo la connessione libero memoria e scarto il fd
                            perror("collector read: message");
                            close_connection(i, &set, &max_fd);
                            SAFE_FREE(str);
                            continue;
                        }
                        //leggo il risultato
                        if ((readn_res = readn(i, &result, sizeof(long))) <= 0)
                        {
                            //se c'è un errore chiudo la connessione libero memoria e scarto il fd
                            perror("collector read: result");
                            close_connection(i, &set, &max_fd);
                            SAFE_FREE(str);
                            continue;
                        }
                        //rispondo che ho ricevuto tutto
                        if ((n = writen(i, &ok, sizeof(int))) == -1)
                        {
                            //se c'è un errore chiudo la connessione libero memoria e scarto il fd
                            perror("collector write: OK file received");
                            close_connection(i, &set, &max_fd);
                            SAFE_FREE(str);
                            continue;
                        }
                        //inserisco filename e risultato nella lista ordinata 
                        insert(&output_queue, result, str, 1); 
                        //libero la memoria allocata
                        SAFE_FREE(str);
                    }
                    else
                    {
                        //se c'è un errore chiudo la connessione libero memoria e scarto il fd
                        close_connection(i, &set, &max_fd);
                    }
                }
            }
        }
    }
    // chiudo tutte le connessioni attive rimaste: se tutto è andato bene chiuderò solo listen_fd e sh_pipe[0]
    for (int i = 0; i <= max_fd; i++)
    {
        if (FD_ISSET(i, &set))
        {
            close(i);
        }
        
    }
    //alla fine del collector stampo i risultati ricevuti
    printQueue(output_queue);
    //libero la lista ordinata 
    freeQueue(&output_queue);
}

//funzione per chiudere la connessione con un client e non ascoltarlo più
static void close_connection(int fd, fd_set *set, int *max_fd)
{
    //chiudo file descriptor
    close(fd);
    //tolgo il fd dal set di ascolto del selector
    FD_CLR(fd, set);
    //se il fd era il masssimo, dovrò aggiornare il massimo
    if (fd == *max_fd)
    {
        //decremento progressivamente il massimo
        while (FD_ISSET(*max_fd, set) == 0)
            *max_fd -= 1;
    }
    //decremento il counter di client attivi
    active_clients--;
}