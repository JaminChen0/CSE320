#include "debug.h"
#include "client_registry.h"
#include "transaction.h"
#include "store.h"
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "server.h"

static void terminate(int status);
void sighup_handler(int signum);

CLIENT_REGISTRY *client_registry;


int main(int argc, char* argv[]){
    //argv

    int port = 1234;
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {

            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -p port \n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (port == 0) {
        fprintf(stderr,"Port number is required\n");
        exit(EXIT_FAILURE);
    }


    struct sigaction act ;
    act.sa_handler=sighup_handler;
    sigemptyset(&act.sa_mask);
    //wiil restart if get iterrupt by read write of accept
    act.sa_flags = SA_RESTART;
    if (sigaction(SIGHUP, &act, NULL) == -1) {
        perror("sigaction SIGHUP");
        exit(EXIT_FAILURE);
    }



    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initializations of the client_registry,
    // transaction manager, and object store.
    client_registry = creg_init();
    trans_init();
    store_init();
    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function xacto_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.




    //socket-> bind -> listen           -> accept          -> recv -> close
    //socket                  -> connect           -> send         -> close
    //ipv4 int domain, int type, int protocol
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    //sockt internet
    serveraddr.sin_family = AF_INET;
    //host to network long
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);  //0.0.0.0 any address
    serveraddr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, 1024) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", port);

    //accept loop
    while (1) {
        struct sockaddr_in clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (connfd < 0) {
            perror("accept failed");
            continue;
        }

        // Handle each client connection in a separate thread
        pthread_t tid;
        int *connfdp = malloc(sizeof(int));
        *connfdp = connfd;
        pthread_create(&tid, NULL, xacto_client_service, connfdp);
    }

    //come here will be a error
    close(listenfd);
    //return 0;

    terminate(EXIT_FAILURE);
}


/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    //Shutdown all client connections.
    //This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    trans_fini();
    store_fini();

    debug("Xacto server terminating");
    exit(status);
}

void sighup_handler(int signum) {
    printf("Received SIGHUP signal. Shutting down...\n");
    terminate(EXIT_SUCCESS);
}