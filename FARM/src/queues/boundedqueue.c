
#include <util.h>
#include <boundedqueue.h>

/**
 * @file boundedqueue.c
 * @brief File di implementazione dell'interfaccia per la coda di dimensione finita
 * @attention questo file è stato modificato per implementare un protocollo produttore consumatore personalizzato esterno al file
 */

/* ------------------- interfaccia della coda ------------------ */

BQueue_t *initBQueue(size_t n)
{
    BQueue_t *q = (BQueue_t *)calloc(sizeof(BQueue_t), 1);
    if (!q)
    {
        perror("malloc");
        return NULL;
    }
    q->buf = calloc(sizeof(void *), n);
    if (!q->buf)
    {
        perror("malloc buf");
        if (!q)
            return NULL;
        int myerrno = errno;
        if (q->buf) {
            SAFE_FREE(q->buf);
        }
        SAFE_FREE(q);
        errno = myerrno;
        return NULL;
    }
    q->head = q->tail = 0;
    q->qlen = 0;
    q->qsize = n;
    return q;
}

void deleteBQueue(BQueue_t *q, void (*F)(void *))
{
    if (!q)
    {
        errno = EINVAL;
        return;
    }
    if (F)
    {
        void *data = NULL;
        while ((data = pop(q)))
            F(data);
    }
    if (q->buf) {
        SAFE_FREE(q->buf);
    }
    SAFE_FREE(q);
}

int push(BQueue_t *q, void *data)
{
    if (!q || !data)
    {
        errno = EINVAL;
        return -1;
    }
    assert(q->buf[q->tail] == NULL);
    q->buf[q->tail] = data;
    q->tail += (q->tail + 1 >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen += 1;
    return 0;
}

void *pop(BQueue_t *q)
{
    if (!q)
    {
        errno = EINVAL;
        return NULL;
    }
    void *data = q->buf[q->head];
    q->buf[q->head] = NULL;
    q->head += (q->head + 1 >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen -= 1;
    assert(q->qlen >= 0);
    return data;
}
