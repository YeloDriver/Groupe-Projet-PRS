#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

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

    struct sockaddr_in control_addresse, control_client, msg_addresse, msg_client;

    int control_port = atoi(argv[1]);
    int valid = 1;
    int msg_port = control_port + 1;
    int send_id = 0;
    int receive_id = 0;
    int file_name_size=32;

    char file_name[file_name_size];
    char buffer[RCVSIZE];
    char msg_port_char[5];
    sprintf(msg_port_char, "%d", msg_port);
    char SYN[] = "SYN";
    char SYNACK_port[13] = "SYN-ACK ";
    for (int i = 0; i <= 4; i++) {
        SYNACK_port[i+8] = msg_port_char[i];
    }
    char ACK[] = "ACK";

    int fd;
    fd_set fds;
    struct timeval timeout;

    socklen_t alen = sizeof(control_addresse);

    //create socket
    int control_server = socket(AF_INET, SOCK_DGRAM, 0);
    int msg_server = socket(AF_INET, SOCK_DGRAM, 0);

    //handle error
    if (msg_server < 0) {
        perror("Cannot create msg socket\n");
        return -1;
    }
    if (control_server < 0) {
        perror("Cannot create control socket\n");
        return -1;
    }

    setsockopt(msg_server, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
    setsockopt(control_server, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

    control_addresse.sin_family = AF_INET;
    control_addresse.sin_port = htons(control_port);
    control_addresse.sin_addr.s_addr = htonl(INADDR_ANY);

    msg_addresse.sin_family = AF_INET;
    msg_addresse.sin_port = htons(msg_port);
    msg_addresse.sin_addr.s_addr = htonl(INADDR_ANY);

    //initialize socket
    if (bind(msg_server, (struct sockaddr *) &msg_addresse, alen) == -1) {
        perror("msg server bind failed\n");
        close(msg_server);
        return -1;
    }

    if (bind(control_server, (struct sockaddr *) &control_addresse, alen) == -1) {
        perror("control server bind failed\n");
        close(control_server);
        return -1;
    }

    printf("Value of socket control UDP is:%d\n", control_server);

    recvfrom(control_server, buffer, sizeof(buffer), 0, (struct sockaddr *) &control_client, &alen);
    if (strcmp(buffer, SYN) != 0) {
        return -1;
    }
    printf("SYN Received\n");

    int synack = sendto(control_server, SYNACK_port, RCVSIZE,
                        0, (struct sockaddr *) &control_client, alen);
    if (synack < 0)
        perror("SYN-ACK send failed");
    printf("SYN-ACK Sended\n");

    recvfrom(control_server, buffer, sizeof(buffer), 0, (struct sockaddr *) &control_client, &alen);
    if (strcmp(buffer, ACK) != 0) {
        perror("ACK error");
        return -1;
    }
    printf("ACK Received\n");

    recvfrom(msg_server, (char *) &file_name, file_name_size, 0, (struct sockaddr *) &msg_client, &alen);
    printf("needed file name is %s\n", file_name);
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL) {
        perror("open file error");
        exit(-1);
    }
    printf("file opened\n");
    int len;
    while (1) {
        FD_ZERO(&fds);
        FD_SET(control_server,&fds);
        FD_SET(msg_server,&fds);
        timeout.tv_sec=0;
        timeout.tv_usec=500;


        PackInfo pack_info;
        if (receive_id == send_id) { //可能是把这个判断逻辑替换成select
            send_id++;
            if ((len = (int) fread(data.buf, sizeof(char), RCVSIZE, fp)) > 0) {
                data.head.id = send_id;
                data.head.buf_size = len;
                printf("fragment %d read\n", send_id);
                if (sendto(msg_server, (char *) &data, sizeof(data),
                           0, (struct sockaddr *) &msg_client, alen) < 0) {
                    perror("fragment send error");
                    break;
                }
                printf("fragment %d sent\n", send_id);
                recvfrom(msg_server, (char *) &pack_info, sizeof(pack_info), 0, (struct sockaddr *) &msg_client,
                         &alen);
                receive_id = pack_info.id;
                printf("fragment %d ACKed\n", receive_id);
            } else {
                printf("file end\n");
                bzero(&data, sizeof(data));
                data.buf[0] = 'E';
                data.buf[1] = 'O';
                data.buf[2] = 'F';
                if (sendto(msg_server, (char *) &data, sizeof(data),
                           0, (struct sockaddr *) &msg_client, alen) < 0) {
                    perror("END send error");
                    break;
                }
                break;
            }
        } else {
            printf("fragment %d resend", send_id);
            if (sendto(msg_server, (char *) &data, sizeof(data),
                       0, (struct sockaddr *) &msg_client, alen) < 0) {
                perror("fragment resend error");
                break;
            }
            recvfrom(msg_server, (char *) &pack_info, sizeof(pack_info), 0, (struct sockaddr *) &msg_client,
                     &alen);
            receive_id = pack_info.id;
        }
    }

    fclose(fp);
    close(msg_server);
    printf("File : %s Transfer Successful!\n", file_name);
}