#ifndef MASTERWORKER_H_
#define MASTERWORKER_H_
//includo alcuni headers
#include <conn.h>
#include <boundedqueue.h>
#include <orderedqueue.h>
#include <util.h>
#include <Collector.h>
//variabili dichiarate esterne per la visibilità nei file Master.c e Worker.c
extern int NUM_THREAD;
extern int Q_LENGHT;
extern int DELAY_TIME;
extern BQueue_t* coda;
extern volatile int sig_interrupted;
extern pthread_mutex_t sig_interrupted_mtx;
extern pthread_mutex_t end_workers_mtx;
extern pthread_mutex_t queue_mtx;
extern pthread_cond_t  queue_cond_empty;
extern pthread_cond_t  queue_cond_full;
extern volatile int end_workers;
extern Queue toSend;
extern volatile int worker_counter;
extern pthread_mutex_t worker_counter_mtx;

//struct da passare come argomento al thread master:
//contiene argc e argv del main Masterworker.c
typedef struct master_args
{
    int argc; //argc
    char **argv; //argv
    
} t_args;

//struct da passare come argomento al thread signal handler:
//contiene la maschera dei segnali e l'end di scrittura della pipe MW-Collector
typedef struct { 
    sigset_t     *set;           // maschera dei segnali da ascoltare
    int           signal_pipe;   // end di scrittura della pipe MW-Collector
} sh_args;

//THREAD MASTER
void* Master(void* args);

//funzione utile per i thread Master e Worker: controlla in maniera thread safe se la variabile flag è stata settata a uno
int check_termination(volatile int *flag, pthread_mutex_t* mtx);

#endif