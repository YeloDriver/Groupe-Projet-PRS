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

#define RCVSIZE 1500
#define ACKSIZE 10

typedef struct
{
    int id;
    int buf_size;
} PackInfo;

struct Sendpack
{
    PackInfo head;
    char buf[RCVSIZE];
} data;

float debits[4];

int main(int argc, char *argv[])
{
    struct sockaddr_in listen_addr, msg_addr, listen_client, msg_client;
    int listen_port = atoi(argv[1]);
    int msg_port = 2000;
    int file_name_size = 32;
    char buffer[RCVSIZE];
    char file_name[file_name_size];
    FILE *fileP = fopen("debit.txt", "w");
    char msg_port_char[5];
    char SYN[] = "SYN";
    char ACK[] = "ACK";
    char FIN[] = "FIN";
    fd_set fd;

    //create listen socket
    int listen_socket = socket(AF_INET, SOCK_DGRAM, 0);
    int msg_socket;

    //handle error
    if (listen_socket < 0)
    {
        perror("Cannot create listen socket\n");
        return -1;
    }

    //initialize socket
    memset((char *)&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(listen_port);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_socket, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) == -1)
    {
        perror("listen socket bind failed\n");
        close(listen_socket);
        return -1;
    }

    printf("Value of listen socket UDP is:%d\n", listen_socket);

    while (1)
    {
        // Initialiser une nouvelle connexion de client
        struct sockaddr_in listen_client_addr;
        memset((char *)&listen_client, 0, sizeof(listen_client_addr));
        socklen_t len_listen_client_addr = sizeof(listen_client_addr);

        bzero(buffer, sizeof(buffer));
        recvfrom(listen_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&listen_client, &len_listen_client_addr);
        if (strcmp(buffer, SYN) != 0)
        {
            return -1;
        }
        printf("SYN Received\n");

        msg_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (msg_socket < 0)
        {
            perror("Cannot create msg socket\n");
            return -1;
        }

        memset((char *)&msg_addr, 0, sizeof(msg_addr));
        msg_addr.sin_family = AF_INET;
        msg_addr.sin_port = htons(msg_port);
        msg_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        struct sockaddr_in msg_client_addr;
        memset((char *)&msg_client, 0, sizeof(msg_client_addr));
        socklen_t len_msg_client_addr = sizeof(msg_client_addr);

        if (bind(msg_socket, (struct sockaddr *)&msg_addr, sizeof(msg_addr)) == -1)
        {
            perror("msg socket bind failed\n");
            close(msg_socket);
            return -1;
        }

        //Estimer RTT
        struct timeval t1, t2, t_debut, t_fin;
        gettimeofday(&t1, NULL);

        sprintf(msg_port_char, "%d", msg_port);
        char SYNACK_port[12] = "SYN-ACK";
        for (int i = 0; i <= 4; i++)
        {
            SYNACK_port[i + 7] = msg_port_char[i];
        }
        int synack = sendto(listen_socket, SYNACK_port, RCVSIZE,
                            0, (struct sockaddr *)&listen_client, sizeof(listen_client));
        if (synack < 0)
        {
            perror("SYN-ACK send failed");
        }
        printf("SYN-ACK Sended\n");

        bzero(buffer, sizeof(buffer));
        recvfrom(listen_socket, buffer, RCVSIZE, 0, (struct sockaddr *)&listen_client, &len_listen_client_addr);
        if (strcmp(buffer, ACK) != 0)
        {
            perror("ACK error");
            return -1;
        }
        gettimeofday(&t2, NULL);

        printf("ACK Received\n");

        msg_port += 1;

        if (fork() == 0)
        {
            close(listen_socket);
            char buffer_msg[RCVSIZE];
            char buffer_ack[ACKSIZE];
            double temps_utile;
            double debit;
            bzero(buffer_msg, RCVSIZE);
            recvfrom(msg_socket, (char *)&file_name, file_name_size, 0, (struct sockaddr *)&msg_client, &len_msg_client_addr);
            printf("needed file name is %s\n", file_name);
            FILE *fp = fopen(file_name, "r");
            if (fp == NULL)
            {
                perror("open file error");
                exit(-1);
            }
            printf("file opened\n");

            fseek(fp, 0, SEEK_END); //Mettre curseur a la fin
            int sizeoffile = ftell(fp);
            fseek(fp, 0, SEEK_SET);                     //Mettre curseur au debut
            int times = sizeoffile / (RCVSIZE - 6) + 1; //times pour envoyer

            int window_size = 1;
            int window_head = 1;
            int window_tail;
            if (window_size < times)
            {
                window_tail = window_head + window_size - 1;
            }
            else
            {
                window_tail = times;
            }
            int last_ack = 0;   //已经获得的最新的ack
            int ack_obtenu = 0; //刚获得的ack
            char seq[7];
            char seq_obtenu[9];
            char file_buffer[RCVSIZE - 6];
            int i = 0;
            char end_of_file[sizeoffile - (RCVSIZE - 6) * (times - 1) + 6];
            int repeat_time = 0;
            int max_repeat_time = 6;
            int old_window_tail = 0;
            int ssthresh = 120;
            int max_window_size = 256;
            struct timeval timeout, new_timeout, old_timeout;
            // Obtenir rtt
            timeout.tv_sec = 1.5 * (t2.tv_sec - t1.tv_sec);
            timeout.tv_usec = 1.5 * (t2.tv_usec - t1.tv_usec);
            struct timeval tableau_timeout[max_window_size];
            double parametre_timeout = 0.5;
            int timeout_time = 0;

            gettimeofday(&t_debut, NULL);
            while (ack_obtenu != times)
            {

                //struct timeval time_now;
                if (last_ack == ack_obtenu)
                {
                    window_size = 1;
                RETRANSMISSION:
                    memset(tableau_timeout, 0, sizeof(tableau_timeout));
                    window_tail = window_head + window_size - 1;
                    if(window_tail>times){
                        window_tail = times;
                    }

                    for (i = window_head; i < window_tail + 1; i++)
                    {
                        bzero(buffer_msg, RCVSIZE);
                        bzero(seq, sizeof(seq));
                        bzero(seq_obtenu, sizeof(seq_obtenu));
                        sprintf(seq, "%06d", i);
                        bzero(file_buffer, sizeof(file_buffer));
                        bzero(end_of_file, sizeof(end_of_file));
                        if (i == times)
                        {
                            memcpy(&end_of_file[0], seq, 6);
                            fseek(fp, (i - 1) * (RCVSIZE - 6), SEEK_SET);
                            fread(&file_buffer, 1, sizeof(file_buffer), fp);
                            memcpy(&end_of_file[6], &file_buffer, sizeof(end_of_file) - 6);
                            sendto(msg_socket, end_of_file, sizeof(end_of_file), 0, (struct sockaddr *)&msg_client, sizeof(msg_client));
                        }
                        else
                        {
                            memcpy(&buffer_msg[0], seq, 6);
                            fseek(fp, (i - 1) * (RCVSIZE - 6), SEEK_SET);
                            fread(&file_buffer, 1, RCVSIZE - 6, fp);
                            memcpy(&buffer_msg[6], &file_buffer, RCVSIZE - 6);
                            sendto(msg_socket, buffer_msg, RCVSIZE, 0, (struct sockaddr *)&msg_client, sizeof(msg_client));
                        }
                        gettimeofday(&t1, NULL);
                        tableau_timeout[i - last_ack - 1] = t1;

                        //printf("tableau_timeout = %d\n", t1.tv_usec);
                        printf("Send paquet.. = %d\n\n", i);
                    }
                }
                else if (last_ack < ack_obtenu)
                {
                    for (i = 0; i < window_size - ack_obtenu + last_ack; i++)
                    {
                        tableau_timeout[i] = tableau_timeout[i + ack_obtenu - last_ack];
                    }
                    if (window_size < ssthresh)
                    {
                        window_size = window_size * 2;
                    }
                    else
                    {
                        window_size++;
                    }
                    if (window_tail < ack_obtenu)
                    {
                        old_window_tail = ack_obtenu;
                    }
                    else
                    {
                        old_window_tail = window_tail;
                    }

                    window_head = window_head + ack_obtenu - last_ack;

                    if (window_head + window_size - 1 < times)
                    {
                        window_tail = window_head + window_size - 1;
                    }
                    else
                    {
                        window_tail = times;
                    }

                    for (i = old_window_tail + 1; i < window_tail + 1; i++)
                    {
                        bzero(file_buffer, sizeof(file_buffer));
                        bzero(buffer_msg, RCVSIZE);
                        bzero(seq, sizeof(seq));
                        bzero(end_of_file, sizeof(end_of_file));
                        bzero(seq_obtenu, sizeof(seq_obtenu));
                        sprintf(seq, "%06d", i);
                        if (i == times)
                        {
                            memcpy(&end_of_file[0], seq, 6);
                            fseek(fp, (i - 1) * (RCVSIZE - 6), SEEK_SET);
                            fread(&file_buffer, 1, sizeof(file_buffer), fp);
                            memcpy(&end_of_file[6], &file_buffer, sizeof(end_of_file) - 6);
                            sendto(msg_socket, end_of_file, sizeof(end_of_file), 0, (struct sockaddr *)&msg_client, sizeof(msg_client));
                            //printf("%s\n", end_of_file);
                        }
                        else
                        {
                            memcpy(&buffer_msg[0], seq, 6);
                            fseek(fp, (i - 1) * (RCVSIZE - 6), SEEK_SET);
                            fread(&file_buffer, 1, RCVSIZE - 6, fp);
                            memcpy(&buffer_msg[6], &file_buffer, RCVSIZE - 6);
                            sendto(msg_socket, buffer_msg, RCVSIZE, 0, (struct sockaddr *)&msg_client, sizeof(msg_client));
                        }
                        gettimeofday(&t1, NULL);
                        tableau_timeout[i - ack_obtenu - 1] = t1;
                        printf("tableau_timeout index.. = %d ", i - ack_obtenu - 1);

                        printf("Send paquet = %d\n\n", i);
                    }
                    last_ack = ack_obtenu;
                }
                else
                {
                    printf("ACK recu est %d < ACK on a, donc on fait rien, juste attend prochain ACK.\n\n", ack_obtenu);
                    ack_obtenu = last_ack;
                }
                for (i = 0; i < 10; i++)
                {
                    printf("[ %ld ], ", tableau_timeout[i].tv_usec);
                }
                printf("  window_head = %d window_tail = %d, window_size = %d\n\n", window_head, window_tail, window_size);
                //printf("last_ack = %d\n", last_ack);

                while (repeat_time < max_repeat_time && ack_obtenu == last_ack)
                {
                    //printf("jinwhile\n");
                    int ret = 0;
                    FD_ZERO(&fd);
                    FD_SET(msg_socket, &fd);
                    old_timeout = timeout;
                    printf("timeout = %ld\n", timeout.tv_usec);
                    ret = select(msg_socket + 1, &fd, NULL, NULL, &timeout);
                    if (ret < 0)
                    {
                        printf("Select error!!!\n");
                    }
                    else if (ret == 0)
                    {
                        printf("Timeout!!! Retransmettre\n\n");
                        timeout_time++;
                        window_size = window_size / 2 + 1;
                        timeout.tv_usec = old_timeout.tv_usec * (1 + 0.5 / timeout_time);
                        timeout.tv_sec = old_timeout.tv_sec * (1 + 0.5 / timeout_time);
                        goto RETRANSMISSION;
                    }
                    bzero(buffer_ack, ACKSIZE);
                    if (FD_ISSET(msg_socket, &fd))
                    {
                        timeout_time = 0;
                        recvfrom(msg_socket, buffer_ack, sizeof(buffer_ack), 0, (struct sockaddr *)&msg_client_addr, &len_msg_client_addr);
                        gettimeofday(&t2, NULL);
                    }

                    for (int i = 3; i < 10; i++)
                    {
                        seq_obtenu[i - 3] = buffer_ack[i];
                    }
                    ack_obtenu = atoi(seq_obtenu);

                    if (ack_obtenu == last_ack)
                    {
                        repeat_time++;
                    }
                    timeout = old_timeout;
                }
                
                repeat_time = 0;

                if (ack_obtenu < window_tail + 1 && ack_obtenu >= window_head && ack_obtenu > last_ack)
                {
                    new_timeout.tv_sec = t2.tv_sec - tableau_timeout[ack_obtenu - window_head].tv_sec;
                    new_timeout.tv_usec = t2.tv_usec - tableau_timeout[ack_obtenu - window_head].tv_usec;
                    if (new_timeout.tv_usec < 0)
                    {
                        new_timeout.tv_sec--;
                        new_timeout.tv_usec = 1000000 + new_timeout.tv_usec;
                    }
                    printf("new_timeout = %ld t2.tv.sec = %ld t2.tv.usec = %ld t1 = %ld index = %d, ack_obtenu = %d, window_head = %d, window_size = %d\n", new_timeout.tv_usec,
                           t2.tv_sec, t2.tv_usec, tableau_timeout[ack_obtenu - window_head].tv_usec, ack_obtenu - window_head, ack_obtenu, window_head, window_size);

                    timeout.tv_usec = parametre_timeout * old_timeout.tv_usec + (1 - parametre_timeout) * new_timeout.tv_usec;
                }
                else
                {
                    timeout = old_timeout;
                }

                printf("Recevoir ACK = %d\n\n", ack_obtenu);
            }

            sendto(msg_socket, FIN, RCVSIZE, 0, (struct sockaddr *)&msg_client, sizeof(msg_client));

            printf("File : %s Transfer Successful!\n\n", file_name);
            gettimeofday(&t_fin, NULL);
            temps_utile = t_fin.tv_sec - t_debut.tv_sec + 0.000001 * (t_fin.tv_usec - t_debut.tv_usec);
            debit = sizeoffile / temps_utile;

            FILE *fileP = fopen("debit.txt", "a");
            fprintf(fileP, "Debit = %03f MB/s\n", debit / (1024 * 1024));
            fclose(fileP);

            printf("Debit = %03f MB/s\n", debit / (1024 * 1024));
            fclose(fp);

            close(msg_socket);
        }
        close(msg_socket);
    }
}