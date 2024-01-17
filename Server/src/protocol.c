#include "protocol.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include "debug.h"
int proto_send_packet(int fd, XACTO_PACKET *pkt, void *data) {
    debug("Calling send packets");
    //  uint32_t  need to transfer
    /*pkt->size = htonl(pkt->size);
    pkt->serial = htonl(pkt->serial);
    pkt->timestamp_sec = htonl(pkt->timestamp_sec);
    pkt->timestamp_nsec = htonl(pkt->timestamp_nsec);*/

    //head
    ssize_t bytes_sent = write(fd, pkt, sizeof(XACTO_PACKET));
    if (bytes_sent < sizeof(XACTO_PACKET)) {
        return -1;
    }

    //data
    if (ntohl(pkt->size) > 0 && data != NULL) {
        size_t total_sent = 0;
        //if didn't send all the data
        while (total_sent < ntohl(pkt->size)) {
            bytes_sent = write(fd, data + total_sent, ntohl(pkt->size) - total_sent);
            if (bytes_sent < 0) {
                return -1;
            }
            total_sent += bytes_sent;
        }
    }
/*
    pkt->size = ntohl(pkt->size);
    pkt->serial = ntohl(pkt->serial);
    pkt->timestamp_sec = ntohl(pkt->timestamp_sec);
    pkt->timestamp_nsec = ntohl(pkt->timestamp_nsec);*/

    return 0;
}

int proto_recv_packet(int fd, XACTO_PACKET *pkt, void **datap) {
    ssize_t bytes_received = read(fd, pkt, sizeof(XACTO_PACKET));
    if (bytes_received < sizeof(XACTO_PACKET)) {
        return -1;
    }
    pkt->size = ntohl(pkt->size);
    pkt->serial = ntohl(pkt->serial);
    pkt->timestamp_sec = ntohl(pkt->timestamp_sec);
    pkt->timestamp_nsec = ntohl(pkt->timestamp_nsec);

    if (pkt->size > 0 &&datap != NULL) {
        *datap = (void*) malloc(pkt->size);
        if (*datap == NULL) {
            return -1;
        }

        size_t total_received = 0;
        while (total_received < pkt->size) {
            //keep receice from last received
            bytes_received = read(fd, *datap + total_received, pkt->size - total_received);

            total_received += bytes_received;
        }
    }
    /*else {
        datap = NULL;
    }*/
    pkt->size = htonl(pkt->size);
    pkt->serial = htonl(pkt->serial);
    pkt->timestamp_sec = htonl(pkt->timestamp_sec);
    pkt->timestamp_nsec = htonl(pkt->timestamp_nsec);
    return 0;
}
