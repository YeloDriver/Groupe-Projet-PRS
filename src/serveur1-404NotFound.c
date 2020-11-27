#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define RCVSIZE 1024

typedef struct {
    int id;
    int buf_size;
} PackInfo;

struct Sendpack {
    PackInfo head;
    char buf[RCVSIZE];
} data;

int main(int argc, char *argv[]) {

    struct sockaddr_in listen_addr, msg_addr,listen_client,msg_client ;
    int listen_port = atoi(argv[1]);
    int msg_port = 6000;
    int file_name_size=32;
    
    char file_name[file_name_size];
    char msg_port_char[5];
    sprintf(msg_port_char, "%d", msg_port);
    char buffer[RCVSIZE];
    char SYN[] = "SYN";
    char SYNACK_port[12] = "SYN-ACK";
    for (int i = 0; i <= 4; i++) {
        SYNACK_port[i+7] = msg_port_char[i];
    }
    char ACK[] = "ACK";
    

    //create listen socket
    int listen_socket = socket(AF_INET, SOCK_DGRAM, 0);
    int msg_socket = socket(AF_INET, SOCK_DGRAM, 0);

    //handle error
    if (listen_socket < 0) {
        perror("Cannot create listen socket\n");
        return -1;
    }
    if (msg_socket < 0) {
        perror("Cannot create msg socket\n");
        return -1;
    }

    //initialize socket
    
    memset((char*)&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    memset((char*)&msg_addr, 0, sizeof(msg_addr));
    msg_addr.sin_family = AF_INET;
    msg_addr.sin_port = htons(msg_port);
    msg_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct sockaddr_in listen_client_addr;
    memset((char*)&listen_client, 0, sizeof(listen_client_addr));
    socklen_t len_listen_client_addr = sizeof(listen_client_addr);
    
    if (bind(listen_socket, (struct sockaddr *) &listen_addr, sizeof(listen_addr)) == -1) {
        perror("listen socket bind failed\n");
        close(listen_socket);
        return -1;
    }

    if (bind(msg_socket, (struct sockaddr *) &msg_addr, sizeof(msg_addr)) == -1) {
        perror("msg socket bind failed\n");
        close(msg_socket);
        return -1;
    }

    printf("Value of listen socket UDP is:%d\n", listen_socket);

    recvfrom(listen_socket, buffer, sizeof(buffer), 0, (struct sockaddr *) &listen_client, &len_listen_client_addr);
    if (strcmp(buffer, SYN) != 0) {
        return -1;
    }
    printf("SYN Received\n");

    int synack = sendto(listen_socket, SYNACK_port, RCVSIZE,
                        0, (struct sockaddr *) &listen_client, sizeof(listen_client));
    if (synack < 0)
        perror("SYN-ACK send failed");
    printf("SYN-ACK Sended\n");

    recvfrom(listen_socket, buffer, sizeof(buffer), 0, (struct sockaddr *) &listen_client, &len_listen_client_addr);
    if (strcmp(buffer, ACK) != 0) {
        perror("ACK error");
        return -1;
    }
    printf("ACK Received\n");

    recvfrom(msg_socket, (char *) &file_name, file_name_size, 0, (struct sockaddr *) &msg_client, &len_listen_client_addr);
    printf("needed file name is %s\n", file_name);
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL) {
        perror("open file error");
        exit(-1);
    }
    printf("file opened\n");





    fclose(fp);
    close(msg_socket);
    printf("File : %s Transfer Successful!\n", file_name);
}
