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
#define ACKSIZE 9


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
    int msg_port = 2000;
    int file_name_size = 32;
    char buffer[RCVSIZE];
    char file_name[file_name_size];
    char msg_port_char[5];
    sprintf(msg_port_char, "%d", msg_port);
    char SYN[] = "SYN";
    char SYNACK_port[12] = "SYN-ACK";
    for (int i = 0; i <= 4; i++) {
        SYNACK_port[i+7] = msg_port_char[i];
    }
    char ACK[] = "ACK";
    char FIN[] = "FIN";
    

    //create listen socket
    int listen_socket = socket(AF_INET, SOCK_DGRAM, 0);
    int msg_socket; 

    //handle error
    if (listen_socket < 0) {
        perror("Cannot create listen socket\n");
        return -1;
    }

    //initialize socket   
    memset((char*)&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct sockaddr_in listen_client_addr;
    memset((char*)&listen_client, 0, sizeof(listen_client_addr));
    socklen_t len_listen_client_addr = sizeof(listen_client_addr);
    
    if (bind(listen_socket, (struct sockaddr *) &listen_addr, sizeof(listen_addr)) == -1) {
        perror("listen socket bind failed\n");
        close(listen_socket);
        return -1;
    }

    printf("Value of listen socket UDP is:%d\n", listen_socket);

    while (1){
        // Initialiser une nouvelle connexion de client
        recvfrom(listen_socket, buffer, sizeof(buffer), 0, (struct sockaddr *) &listen_client, &len_listen_client_addr);    
        if (strcmp(buffer, SYN) != 0) {
            return -1;
        }
        printf("SYN Received\n");

        msg_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (msg_socket < 0) {
            perror("Cannot create msg socket\n");
        return -1;
        }

        memset((char*)&msg_addr, 0, sizeof(msg_addr));
        msg_addr.sin_family = AF_INET;
        msg_addr.sin_port = htons(msg_port);
        msg_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        msg_port += 1;

        struct sockaddr_in msg_client_addr;
        memset((char*)&msg_client, 0, sizeof(msg_client_addr));
        socklen_t len_msg_client_addr = sizeof(msg_client_addr);

        if (bind(msg_socket, (struct sockaddr *) &msg_addr, sizeof(msg_addr)) == -1) {
            perror("msg socket bind failed\n");
            close(msg_socket);
            return -1;
        }

        //Estimer RTT
        struct timeval t1,t2,timeout;
        gettimeofday(&t1,NULL);
        int synack = sendto(listen_socket, SYNACK_port, RCVSIZE,
                            0, (struct sockaddr *) &listen_client, sizeof(listen_client));
        if (synack < 0){
            perror("SYN-ACK send failed");
        }
        printf("SYN-ACK Sended\n");

        recvfrom(listen_socket, buffer, RCVSIZE, 0, (struct sockaddr *) &listen_client, &len_listen_client_addr);
        if (strcmp(buffer, ACK) != 0) {
            perror("ACK error");
            return -1;
        }
        gettimeofday(&t2,NULL);
        // Obtenir rtt
        timeout.tv_sec = t2.tv_sec-t1.tv_sec;
        timeout.tv_usec = t2.tv_usec-t1.tv_usec;
        
        printf("ACK Received\n");

        if(fork()==0){
            char buffer_msg[RCVSIZE];
            char buffer_ack[ACKSIZE];
            bzero(buffer_msg, RCVSIZE);
            recvfrom(msg_socket, (char *) &file_name, file_name_size, 0, (struct sockaddr *) &msg_client, &len_msg_client_addr);
            printf("needed file name is %s\n", file_name);
            FILE *fp = fopen(file_name, "r");
            if (fp == NULL) {
                perror("open file error");
                exit(-1);
            }
            printf("file opened\n");

            fseek(fp,0,SEEK_END);//Mettre curseur a la fin
            int sizeoffile = ftell(fp);
            fseek(fp,0,SEEK_SET);//Mettre curseur au debut
            int times = sizeoffile/(RCVSIZE-6) + 1; //times pour envoyer

            int last_ack = 1;
            int ack_obtenu = 1;
            int window_size = 400;
            char seq[6];
            char seq_obtenu[6];
            char payload[RCVSIZE-6];

            //Set timeout on socket
            //setsockopt(msg_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            
            //Commence a transmission
            while(last_ack!=times){
                if(last_ack<ack_obtenu){
                    bzero(seq, sizeof(seq));
                    bzero(payload, sizeof(payload));
                    fread(payload, 1, sizeof(payload), fp);
                    sprintf(seq, "%06d", last_ack);
                    memcpy(&buffer_msg[0], seq, 6);
                    memcpy(&buffer_msg[6], payload, sizeof(payload));
                    printf("%s\n", buffer_msg);
                    
                    //bzero(buffer_msg, sizeof(buffer_msg));
                    sendto(msg_socket, buffer_msg, RCVSIZE, 0, (struct sockaddr*)&msg_client, sizeof(msg_client));
                    int size = recvfrom(msg_socket, buffer_ack, ACKSIZE, 0 , (struct sockaddr*)&msg_client, &len_msg_client_addr); 
                    printf("%d\n", size);
                    printf("%s\n", buffer_ack);

                    memcpy(&buffer_ack[3], seq_obtenu, 6);
                    ack_obtenu = atoi(seq_obtenu);

                    printf("%d\n",ack_obtenu);
                }else{
                    sendto(msg_socket, buffer_msg, sizeof(buffer_msg), 0, (struct sockaddr*)&msg_addr, sizeof(msg_addr));
                    recvfrom(msg_socket, buffer_ack, sizeof(buffer_ack), MSG_DONTWAIT , (struct sockaddr*)&msg_client_addr, &len_msg_client_addr); 
                    memcpy(&buffer_ack[3], seq_obtenu, 6);
                    ack_obtenu = atoi(seq_obtenu);
                }
                printf("%d\n", last_ack);
            }

            sendto(msg_socket, FIN, RCVSIZE, 0, (struct sockaddr*)&msg_client, sizeof(msg_client));
            fclose(fp);
            close(msg_socket);
            printf("File : %s Transfer Successful!\n", file_name);
        }
    }
    
    
}
