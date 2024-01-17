#include "client_registry.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/select.h>
#include "debug.h"

typedef struct client_registry {
    int client_fds[FD_SETSIZE];
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} CLIENT_REGISTRY;

CLIENT_REGISTRY *creg_init() {
    debug("Initialize client registry");
    CLIENT_REGISTRY *cr = malloc(sizeof(CLIENT_REGISTRY));
    if (cr == NULL) {
        return NULL;
    }

    cr->count = 0;
    pthread_mutex_init(&cr->mutex, NULL);
    pthread_cond_init(&cr->cond, NULL);
    return cr;
}


//desotry everything
void creg_fini(CLIENT_REGISTRY *cr) {
    if (cr != NULL) {
        pthread_mutex_destroy(&cr->mutex);
        pthread_cond_destroy(&cr->cond);
        //free(cr->client_fds);
        free(cr);
    }
}


int creg_register(CLIENT_REGISTRY *cr, int fd) {
    pthread_mutex_lock(&cr->mutex);

    //if max
    if (cr->count >= FD_SETSIZE) {
        pthread_mutex_unlock(&cr->mutex);
        return -1;
    }

    //cr->client_fds = client fds array
    cr->client_fds[cr->count ] = fd;
    cr->count++;
    debug("Register client fd %d (total connected: %d)", fd, cr->count);
    pthread_mutex_unlock(&cr->mutex);
    return 0;
}

int creg_unregister(CLIENT_REGISTRY *cr, int fd) {
    pthread_mutex_lock(&cr->mutex);
    for (int i = 0; i < cr->count; i++) {
        if (cr->client_fds[i] == fd) {
            //move the last fd to the position where we need to remove
            cr->client_fds[i] = cr->client_fds[--cr->count];
            if (cr->count == 0) {
                pthread_cond_signal(&cr->cond);
            }
            pthread_mutex_unlock(&cr->mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&cr->mutex);
    return -1;
}


void creg_shutdown_all(CLIENT_REGISTRY *cr) {
    pthread_mutex_lock(&cr->mutex);
    for (int i = 0; i < cr->count; i++) {
        //close both read and wrtie
        shutdown(cr->client_fds[i], SHUT_RDWR);
    }
    pthread_mutex_unlock(&cr->mutex);
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
    pthread_mutex_lock(&cr->mutex);
    while (cr->count > 0) {
        //sleep(1)
        pthread_cond_wait(&cr->cond, &cr->mutex);//wait
    }
    pthread_mutex_unlock(&cr->mutex);
}
