#include "transaction.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"
TRANSACTION trans_list;

void trans_init(void) {
    pthread_mutex_init(&trans_list.mutex, NULL);
    trans_list.next = &trans_list;
    trans_list.prev = &trans_list;
    debug("Initialize transaction manager");
}

void trans_fini(void) {
    pthread_mutex_destroy(&trans_list.mutex);
}
TRANSACTION *trans_create(void) {
    //printf("im here 1\n");
    static int next_id = 0;
    TRANSACTION *tp = malloc(sizeof(TRANSACTION));
    if (tp == NULL) {
        return NULL;
    }

    tp->id = next_id++;;
    tp->refcnt = 1;
    tp->status = TRANS_PENDING;
    tp->depends = NULL;
    tp->waitcnt = 0;
    sem_init(&tp->sem, 0, 0);
    pthread_mutex_init(&tp->mutex, NULL);
    debug("create new transaction %d", tp->id);

    pthread_mutex_lock(&trans_list.mutex);
    // trans_list ->  tp ->   trans_list.next
    tp->next = trans_list.next;
    tp->prev = &trans_list;
    trans_list.next->prev = tp;
    trans_list.next = tp;
    pthread_mutex_unlock(&trans_list.mutex);
    //printf("im here 1\n");
    return tp;
}

TRANSACTION *trans_ref(TRANSACTION *tp, char *why) {
    //printf("im here 2\n");
    pthread_mutex_lock(&tp->mutex);
    tp->refcnt++;
    debug("Increase ref count on transaction %d (%d -> %d) for %s", tp->id, tp->refcnt-1, tp->refcnt, why);
    pthread_mutex_unlock(&tp->mutex);
    //printf("im here 2\n");
    return tp;
}

void trans_unref(TRANSACTION *tp, char *why) {
    pthread_mutex_lock(&tp->mutex);
    tp->refcnt--;
    if (tp->refcnt == 0) {
        pthread_mutex_lock(&trans_list.mutex);
        tp->prev->next = tp->next;
        tp->next->prev = tp->prev;
        pthread_mutex_unlock(&trans_list.mutex);
        pthread_mutex_unlock(&tp->mutex);
        pthread_mutex_destroy(&tp->mutex);
        sem_destroy(&tp->sem);
        DEPENDENCY *dep = tp->depends;
        while (dep) {
            DEPENDENCY *temp = dep;
            dep = dep->next;
            free(temp);
        }
        free(tp);
    } else {
        debug("Decrease ref count on transaction %d (%d -> %d) for %s", tp->id, tp->refcnt+1, tp->refcnt, why);
        pthread_mutex_unlock(&tp->mutex);
    }
}

void trans_add_dependency(TRANSACTION *tp, TRANSACTION *dtp) {
    //printf("im here 4\n");
    pthread_mutex_lock(&tp->mutex);

    //check if depend
    for (DEPENDENCY *dep = tp->depends; dep != NULL; dep = dep->next) {
        if (dep->trans == dtp) {
            pthread_mutex_unlock(&tp->mutex);
            return;
        }
    }

    DEPENDENCY *new_dep = malloc(sizeof(DEPENDENCY));
    if (new_dep == NULL) {
        pthread_mutex_unlock(&tp->mutex);
        return;
    }
    new_dep->trans = dtp;
    trans_ref(dtp,"Adding dependency");

    new_dep->next = tp->depends;
    tp->depends = new_dep;

    pthread_mutex_unlock(&tp->mutex);
    //printf("im here 4\n");
}

TRANS_STATUS trans_commit(TRANSACTION *tp) {
    //printf("im here 5\n");
    pthread_mutex_lock(&tp->mutex);
    //assert(tp->status == TRANS_PENDING);

    for (DEPENDENCY *dep = tp->depends; dep != NULL; dep = dep->next) {
        TRANSACTION *dtp = dep->trans;
        pthread_mutex_lock(&dtp->mutex);
        tp->waitcnt++;
        pthread_mutex_unlock(&dtp->mutex);

        sem_wait(&dtp->sem);

        if (trans_get_status(dtp) == TRANS_ABORTED) {
            pthread_mutex_unlock(&tp->mutex);
            trans_abort(tp);
            return TRANS_ABORTED;
        }
    }

    tp->status = TRANS_COMMITTED;
    pthread_mutex_unlock(&tp->mutex);

    while (tp->waitcnt > 0) {
        sem_post(&tp->sem);
        tp->waitcnt--;
    }

    trans_unref(tp,"Committing transaction");
    //printf("im here 5\n");
    return TRANS_COMMITTED;
}


TRANS_STATUS trans_abort(TRANSACTION *tp) {
    //printf("im here 6\n");
    pthread_mutex_lock(&tp->mutex);
    if (tp->status == TRANS_COMMITTED) {
        fprintf(stderr,"Attempt to abort a committed transaction\n");
        abort();
    }

    if (tp->status == TRANS_ABORTED) {
        pthread_mutex_unlock(&tp->mutex);
        trans_unref(tp,"Aborting transaction");
        return TRANS_ABORTED;
    }


    tp->status = TRANS_ABORTED;
    pthread_mutex_unlock(&tp->mutex);

    while (tp->waitcnt > 0) {
        sem_post(&tp->sem);
        tp->waitcnt--;
    }

    trans_unref(tp,"Aborting transaction");
    //printf("im here 6\n");
    return TRANS_ABORTED;
}

TRANS_STATUS trans_get_status(TRANSACTION *tp) {
    pthread_mutex_lock(&tp->mutex);
    TRANS_STATUS status = tp->status;
    pthread_mutex_unlock(&tp->mutex);
    return status;
}

void trans_show(TRANSACTION *tp) {
    pthread_mutex_lock(&tp->mutex);
    printf("Transaction ID: %u, Status: %d, Refcount: %u\n", tp->id, tp->status, tp->refcnt);
    pthread_mutex_unlock(&tp->mutex);
}

void trans_show_all(void) {
    pthread_mutex_lock(&trans_list.mutex);
    TRANSACTION *current = trans_list.next;
    while (current != &trans_list) {
        trans_show(current);
        current = current->next;
    }
    pthread_mutex_unlock(&trans_list.mutex);
}
