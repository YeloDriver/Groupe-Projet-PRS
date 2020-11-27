#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

    struct sockaddr_in control_addresse, msg_addresse;
    int control_port = atoi(argv[2]);
    int valid = 1;
    int id = 1;
    int msg_port;
    socklen_t alen = sizeof(control_addresse);
    char blanmsg[RCVSIZE];
    char SYN[] = "SYN";
    char SYNACK[] = "SYN-ACK";
    char ACK[] = "ACK";
    char file_name[] = "GenshinImpact.jpg";
    char msg_port_char[5];
    bzero(msg_port_char, 5);

    //create socket
    int control_server = socket(AF_INET, SOCK_DGRAM, 0);
    int msg_server = socket(AF_INET, SOCK_DGRAM, 0);

    // handle error
    if (control_server < 0) {
        perror("cannot create socket\n");
        return -1;
    }

    if (msg_server < 0) {
        perror("cannot create socket\n");
        return -1;
    }

    setsockopt(control_server, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
    setsockopt(msg_server, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

    control_addresse.sin_family = AF_INET;
    control_addresse.sin_port = htons(control_port);
    control_addresse.sin_addr.s_addr = inet_addr(argv[1]);


    int syn = sendto(control_server, SYN, RCVSIZE,
                     0, (const struct sockaddr *) &control_addresse,
                     alen);
    if (syn < 0) {
        perror("SYN send error\n");
        return -1;
    }
    printf("SYN Sended\n");

    recvfrom(control_server, blanmsg, RCVSIZE,
             0, (struct sockaddr *) &control_addresse,
             &alen);

    if (strncmp(blanmsg, SYNACK, 7) == 0) {
        printf("SYN-ACK Received\n");
        memcpy(msg_port_char, blanmsg + 8, 4);
        msg_port = atoi(msg_port_char);
        int ack = sendto(control_server, ACK, strlen(ACK),
                         0, (const struct sockaddr *) &control_addresse,
                         alen);
        if (ack < 0)
            perror("ACK error");
        printf("ACK Sended\n");
    } else {
        perror("SYN-ACK error");
        return -1;
    }
    printf("control msg port : %d\n", msg_port);

    msg_addresse.sin_family = AF_INET;
    msg_addresse.sin_port = htons(msg_port);
    msg_addresse.sin_addr.s_addr = inet_addr(argv[1]);

    if (sendto(msg_server, &file_name, sizeof(file_name), 0, (const struct sockaddr *) &msg_addresse, alen) < 0) {
        perror("file name send error\n");
    }
    printf("file name %s send correctly\n", file_name);

    FILE *fp = fopen("test.jpg", "w");
    if (fp == NULL) {
        perror("create file error\n");
        exit(-1);
    }
    printf("file created\n");

    int len;
    while (1) {
        PackInfo pack_info;
        len = recvfrom(msg_server, &data, sizeof(data),
                       0, (struct sockaddr *) &msg_addresse,
                       &alen);

        if (len > 0) {
            if (strncmp(data.buf, "EOF", 3) == 0)
                goto here;
            printf("fragment %d received\n", data.head.id);
            if (data.head.id == id) {
                pack_info.id = data.head.id;
                pack_info.buf_size = data.head.buf_size;
                printf("fragment id correct\n");
                id++;
                if (sendto(msg_server, &pack_info, sizeof(pack_info), 0, (const struct sockaddr *) &msg_addresse,
                           alen) < 0) {
                    perror("fragment ACK failed\n");
                }
                printf("fragment %d ACK sent\n", pack_info.id);
                if (fwrite(data.buf, sizeof(char), data.head.buf_size, fp) < data.head.buf_size) {
                    printf("fragment %d write failed\n", pack_info.id);
                    break;
                }
                printf("fragment %d wrote\n", pack_info.id);
            } else if (data.head.id < id) {
                printf("fragment Nb wrong\n");
                printf("pack resent\n");
                pack_info.id = data.head.id;
                pack_info.buf_size = data.head.buf_size;
                if (sendto(msg_server, &pack_info, sizeof(pack_info), 0, (const struct sockaddr *) &msg_addresse,
                           alen) < 0) {
                    perror("send ACK failed\n");
                }
            }
        }
    }
    here:
    printf("file end\n");
    printf("Receive File : %s Successful\n", file_name);
    fclose(fp);
    close(control_server);
    return 0;
}