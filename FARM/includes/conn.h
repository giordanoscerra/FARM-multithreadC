#ifndef CONN_H
#define CONN_H
#include<util.h>
#define SOCKNAME "./farm.sck"


extern int sh_pipe[2];

/* 
	@funzione readn
	@descrizione: evita letture parziali nel file riferito da fd
	@param fd è il file descriptor da dove voglio leggere
	@param buf è il buffer in cui voglio salvare ciò che leggo
	@param size indica quanti bytes voglio leggere
	@return size se termina con successo, 0 se durante la lettura da fd leggo EOF, -1 in caso d'errore setta errno
*/
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;   // EOF
        left    -= r;
	bufptr  += r;
    }
    return size;
}

/* 
	@funzione writen
	@descrizione: evita scritture parziali nel file riferito da fd
	@param fd è il file descriptor dove voglio scrivere
	@param buf è il contenuto che voglio scrivere in fd
	@param size indica quanti bytes voglio scrivere da buf
	@return size se termina con successo, 0 se durante la lettura da fd leggo EOF, -1 in caso d'errore setta errno
*/

static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}
	if (r == 0) return 0;  
        left    -= r;
	bufptr  += r;
    }
    return 1;
}
#endif
