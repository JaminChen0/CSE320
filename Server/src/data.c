#include "data.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "debug.h"

// Create
BLOB *blob_create(char *content, size_t size) {
    //printf("im here 1\n");
    if ((content == NULL) || size == 0) {
        return NULL;
    }

    BLOB *blob = malloc(sizeof(BLOB));
    if (blob == NULL) {
        return NULL;
    }

    blob->content = malloc(size);
    if (blob->content == NULL) {
        //free(blob);
        return NULL;
    }
    memcpy(blob->content, content, size);

    blob->prefix = malloc(sizeof(blob->prefix) );
    memcpy(blob->prefix, blob->content, size);

    //blob->prefix = blob->content;
    blob->size = size;
    blob->refcnt = 1; //////////////////////////////////////
    debug("Create blob with content %p, size %zu -> %p", blob->content, blob->size, blob);
    pthread_mutex_init(&blob->mutex, NULL);

    //printf("im here 1\n");
    return blob;
}

//increase the reference count on a blob
BLOB *blob_ref(BLOB *bp, char *why) {
    //printf("im here 2 \n");
    if (bp == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&bp->mutex);
    (bp->refcnt)++;
    debug("Increase ref count of blob %p (%d -> %d) for %s", bp, bp->refcnt-1, bp->refcnt, why);
    pthread_mutex_unlock(&bp->mutex);
    //printf("im here 2 \n");
    return bp;
}

//decrease the reference count on a blob
void blob_unref(BLOB *bp, char *why) {
    //printf("im here 3 \n");
    if (bp == NULL) {
        return;
    }

    pthread_mutex_lock(&bp->mutex);
    (bp->refcnt)--;
    debug("Decrease ref count of blob %p (%d -> %d) for %s", bp, bp->refcnt+1, bp->refcnt, why);

    if (bp->refcnt == 0) {
        debug("Freeing blob %p for %s", bp, why);
        pthread_mutex_unlock(&bp->mutex);
        pthread_mutex_destroy(&bp->mutex);
        free(bp->prefix);
        free(bp->content);
        free(bp);
    } else {
        pthread_mutex_unlock(&bp->mutex);
    }
    //printf("im here 3 \n");
}

// Compare two blobs for equality of their content
int blob_compare(BLOB *bp1, BLOB *bp2) {
    //printf("im here 4 \n");
    if (bp1 == NULL || bp2 == NULL) {
        return -1;
    }

    if (bp1->size != bp2->size) {
        return -1;
    }
    //printf("im here 4 \n");
    return memcmp(bp1->content, bp2->content, bp1->size) == 0 ? 0 : -1;
}

// Hash function for hashing the content of a blob
int blob_hash(BLOB *bp) {
    //printf("im here 5 \n");
    if (bp == NULL || bp->content == NULL) {
        return -1;
    }

    // Simple hash algorithm
    int hash = 0;
    for (size_t i = 0; i < bp->size; i++) {
        hash = 20 * hash + bp->content[i];
    }
    //printf("im here 5 \n");
    return hash;
}

// Create a key from a blob
KEY *key_create(BLOB *bp) {
    //printf("im here 6 \n");
    if (bp == NULL) {
        return NULL;
    }

    KEY *key = malloc(sizeof(KEY));
    if (key == NULL) {
        return NULL;
    }
    key->blob = bp;
    key->hash = blob_hash(bp);
    debug("create key from blob %p -> %p (%d)", bp, key, key->blob->refcnt);
    //printf("im here 6 \n");
    return key;

}

// Dispose of a key
void key_dispose(KEY *kp) {
    //printf("im here 7 \n");
    if (kp == NULL) {
        return;
    }

    blob_unref(kp->blob, "Disposing key");
    debug("Dispose of key %p (%d)", kp, kp->blob->refcnt);
    free(kp);

    //printf("im here 7 \n");
}

// Compare two keys for equality
int key_compare(KEY *kp1, KEY *kp2) {
    //printf("im here 8 \n");
    if (kp1 == NULL || kp2 == NULL) {
        return -1;
    }

    if (kp1->hash != kp2->hash) {
        return -1;
    }

    //printf("im here 8 \n");

    return blob_compare(kp1->blob, kp2->blob);
}

// Create a version of a blob
VERSION *version_create(TRANSACTION *tp, BLOB *bp) {
    //printf("im here 9 \n");
    /*if (tp == NULL || bp == NULL) {
        if (tp == NULL){
            printf("aaaa\n");
        }
        if (bp == NULL){
            printf("bbbb\n");
        }
        return NULL;
    }*/
    //printf("im here 9 \n");
    VERSION *version = malloc(sizeof(VERSION));
    if (version == NULL) {
        return NULL;
    }


    version->creator = tp;
    version->blob = bp;
    //no sure here
    version->next = NULL;
    version->prev = NULL;

    trans_ref(tp, "Version created");
    //blob_ref(bp,"Version created");

    //printf("im here 9 \n");
    return version;
}

// Dispose of a version
void version_dispose(VERSION *vp) {
    //printf("im here 10 \n");
    if (vp == NULL) {
        return;
    }

    /*if ((vp->prev != NULL) && (vp->next != NULL) ) {
        vp->prev->next = vp->next;
    }
    if ((vp->prev != NULL) && (vp->next != NULL) ) {
        vp->next->prev = vp->prev;
    }*/


    trans_unref(vp->creator, "Disposing version");
    blob_unref(vp->blob, "Disposing version");

    free(vp);
    //printf("im here 10 \n");
}
