#if !defined(_UTIL_H)
#define _UTIL_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <wait.h>
#include <signal.h>
#include <time.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>

// funzione che trova il massimo tra due numeri
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
// safe free per non commettere atrocità
#define SAFE_FREE(pointer)     \
  if (pointer != NULL)         \
  {                            \
    free(pointer);             \
    pointer = NULL;            \
  }                            \
  else                         \
  {                            \
    printf("safe inutile.\n"); \
  }
// lock
#define LOCK(l)                              \
  if (pthread_mutex_lock(l) != 0)            \
  {                                          \
    fprintf(stderr, "ERRORE FATALE lock\n"); \
    pthread_exit((void *)EXIT_FAILURE);      \
  }
// unlock
#define UNLOCK(l)                              \
  if (pthread_mutex_unlock(l) != 0)            \
  {                                            \
    fprintf(stderr, "ERRORE FATALE unlock\n"); \
    pthread_exit((void *)EXIT_FAILURE);        \
  }
// wait
#define WAIT(c, l)                           \
  if (pthread_cond_wait(c, l) != 0)          \
  {                                          \
    fprintf(stderr, "ERRORE FATALE wait\n"); \
    pthread_exit((void *)EXIT_FAILURE);      \
  }
// signal
#define SIGNAL(c)                              \
  if (pthread_cond_signal(c) != 0)             \
  {                                            \
    fprintf(stderr, "ERRORE FATALE signal\n"); \
    pthread_exit((void *)EXIT_FAILURE);        \
  }
// broadcast
#define BCAST(c)                                  \
  if (pthread_cond_broadcast(c) != 0)             \
  {                                               \
    fprintf(stderr, "ERRORE FATALE broadcast\n"); \
    pthread_exit((void *)EXIT_FAILURE);           \
  }

/**
 * \brief Controlla se la stringa passata come primo argomento e' un numero.
 * \return  0 ok  1 non e' un numbero   2 overflow/underflow
 */
static inline int isNumber(const char *s, long *n)
{
  if (s == NULL)
    return 1;
  if (strlen(s) == 0)
    return 1;
  char *e = NULL;
  errno = 0;
  long val = strtol(s, &e, 10);
  if (errno == ERANGE)
    return 2; // overflow/underflow
  if (e != NULL && *e == (char)0)
  {
    *n = val;
    return 0; // successo
  }
  return 1; // non e' un numero
}

#endif /* _UTIL_H */
