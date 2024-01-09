#include "protocol.h"
#include "client_registry.h"
#include "store.h"
#include "transaction.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "debug.h"
extern CLIENT_REGISTRY *client_registry;

void *xacto_client_service(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);

    pthread_detach(pthread_self());
    creg_register(client_registry, client_fd);

    TRANSACTION *transaction = trans_create();
    if (transaction == NULL) {
        close(client_fd);
        creg_unregister(client_registry, client_fd);
        return NULL;
    }

    while (1) {
        XACTO_PACKET packet;
        void *data = NULL;
        if (proto_recv_packet(client_fd, &packet, &data) == -1) {
            trans_abort(transaction);
            break;
        }

        // Convert fields from network byte order to host byte order
        //packet.type = ntohl(packet.type);
        //packet.serial = ntohl(packet.serial);

        XACTO_PACKET reply;
        memset(&reply, 0, sizeof(reply));
        reply.type = XACTO_REPLY_PKT;
        reply.serial = packet.serial;

        switch (packet.type) {
            case XACTO_PUT_PKT: {
                debug("Received PUT request from client %d", client_fd);

                XACTO_PACKET key_packet;
                void *key_data;
                if (proto_recv_packet(client_fd, &key_packet, &key_data) == -1) {
                    trans_abort(transaction);
                    break;
                }



                BLOB *key_blob = blob_create(key_data, ntohl(key_packet.size));
                KEY *key = key_create(key_blob);
                free(key_data);

                XACTO_PACKET value_packet;
                void *value_data;
                if (proto_recv_packet(client_fd, &value_packet, &value_data) == -1) {
                    key_dispose(key);
                    trans_abort(transaction);
                    break;
                }

                BLOB *value_blob = blob_create(value_data, ntohl(value_packet.size) );
                free(value_data); // 释放接收的值数据
                TRANS_STATUS status = store_put(transaction, key, value_blob);
                //blob_unref(value_blob, "PUT operation - unref value blob");
                key_dispose(key);


                XACTO_PACKET reply;
                memset(&reply, 0, sizeof(reply));
                reply.type = XACTO_REPLY_PKT;
                reply.serial = packet.serial; // 使用原始请求的序列号
                reply.status = status; // 设置状态
                if (proto_send_packet(client_fd, &reply, NULL) == -1) {
                    trans_abort(transaction);
                }

                /*//test
                key_packet.size = htonl(key_packet.size);
                value_packet.size = htonl(value_packet.size);
                //test*/


                break;
            }
            case XACTO_GET_PKT: {
                debug("Received GET request from client %d", client_fd);
                XACTO_PACKET key_packet;
                void *key_data;
                if (proto_recv_packet(client_fd, &key_packet, &key_data) == -1) {
                    trans_abort(transaction);
                    break;
                }


                //key_packet.size = ntohl(key_packet.size);
                ///////////////////////////////
                /*if (key_data == NULL || key_packet.size == 0) {
                    printf("nulllllllllllllllllllllllllllllllllll3\n");
                    if(key_data == NULL){
                        printf("value_data null\n");
                    }
                    if(key_packet.size == 0){
                        printf("value_packet.size null\n");
                    }
                }*/
                /*if (key_data == NULL || key_packet.size == 0 || key_packet.size > MAX_BLOB_SIZE) {
                    free(key_data);
                    trans_abort(transaction);
                    break;
                }*/



                // 创建键的 BLOB 和 KEY
                BLOB *key_blob = blob_create(key_data, ntohl(key_packet.size));
                KEY *key = key_create(key_blob);
                //blob_unref(key_blob, "GET operation - unref key blob");
                free(key_data); // 释放接收的键数据

                // 使用 store_get 函数检索值
                BLOB *value_blob = NULL;
                TRANS_STATUS status = store_get(transaction, key, &value_blob);
                key_dispose(key);



                // 发送 REPLY 数据包
                XACTO_PACKET reply;
                memset(&reply, 0, sizeof(reply));
                reply.type = XACTO_REPLY_PKT;
                reply.serial = packet.serial; // 使用原始请求的序列号
                reply.status = status; // 设置状态
                if (proto_send_packet(client_fd, &reply, NULL) == -1) {
                    trans_abort(transaction);
                    break;
                }

                // 如果找到值，发送 VALUE 数据包
                //if (status == TRANS_PENDING && value_blob != NULL) {
                if (value_blob != NULL) {
                    if (proto_send_packet(client_fd, &reply, value_blob->content) == -1) {
                        //blob_unref(value_blob, "GET operation - unref value blob");
                        trans_abort(transaction);
                    }
                }

                if (value_blob != NULL) {
                    //blob_unref(value_blob, "GET operation - unref value blob");
                }



                break;
            }

            case XACTO_COMMIT_PKT: {
                debug("Received COMMIT request from client %d", client_fd);
                reply.status = trans_commit(transaction); // Commit transaction and convert status
                proto_send_packet(client_fd, &reply, NULL); // Send reply
                break;
            }
            default:{
                reply.status = TRANS_ABORTED; // Convert status to network byte order for an unknown packet type
                //proto_send_packet(client_fd, &reply, NULL); // Send reply
                trans_abort(transaction); // Abort transaction
                break;
            }

        }

        // Check the transaction status to decide whether to continue or break the loop
        if (reply.status != TRANS_PENDING) {
            break;
        }

        if (data != NULL) {
            free(data); // Free any allocated data
        }
    }

    // Clean up
    trans_unref(transaction, "Ending client session");
    close(client_fd);
    creg_unregister(client_registry, client_fd);
    return NULL;
}
