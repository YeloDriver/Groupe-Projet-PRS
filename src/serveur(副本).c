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
#include <sys/time.h>
#include <math.h>


int main(int argc, char* argv[]){
    int domaine = AF_INET;
    int type_UDP = SOCK_DGRAM;
    int protocle = 0;
    int err;
    char buffer[1024];

    int select_socket = socket(domaine, type_UDP, protocle);

    struct sockaddr_in select_addr;
    memset((char*)&select_addr, 0, sizeof(select_addr));
    select_addr.sin_family = AF_INET;
    select_addr.sin_port = htons(2333);
    select_addr.sin_addr.s_addr = INADDR_ANY;
    bind(select_socket, (struct sockaddr *) &select_addr, sizeof(select_addr));

    struct sockaddr_in client_addr;
    memset((char*)&client_addr, 0, sizeof(client_addr));
    socklen_t len_client_addr = sizeof(client_addr);


    fd_set fds;
    struct timeval timeout;


    while (1){
        bzero(buffer, sizeof(buffer)); 
        //Etablir TCP connection
        recvfrom(select_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len_client_addr);
        printf("%s\n", buffer);
        if (strcmp(buffer, "SYN")!=0){
            printf("%s\n", "Connect error");
        }else{
            sendto(select_socket, "ACK-SYN", strlen("ACK-SYN"), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        }
        recvfrom(select_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len_client_addr);
        printf("%s\n", buffer);
        if (strcmp(buffer, "ACK")!=0){
            printf("%s\n", "Connect error");
        }else{
            printf("%s\n", "Connect etablit");        
        }

        //Ouverture la nouvelle porte
        int new_port_socket = socket(domaine, type_UDP, protocle);
        struct sockaddr_in new_port_addr;
        memset((char*)&new_port_addr, 0, sizeof(new_port_addr));
        new_port_addr.sin_family = AF_INET;
        new_port_addr.sin_port = htons(6666);
        new_port_addr.sin_addr.s_addr = INADDR_ANY;
        bind(new_port_socket, (struct sockaddr *) &new_port_addr, sizeof(new_port_addr));

        struct sockaddr_in client_addr_control;
        memset((char*)&client_addr_control, 0, sizeof(client_addr_control));
        socklen_t len_client_addr_control = sizeof(client_addr_control);

        sendto(select_socket, "6666", strlen("6666"), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        //recvfrom(new_port_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len_client_addr);
        recvfrom(new_port_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr_control, &len_client_addr_control);
        printf("%s\n", buffer);  
    
        FILE *photo = fopen("/data/home/hello_zehan/INSA/4TC/PRS/TP3/tiger.jpg","r");
        if(photo == NULL){
            printf("%s\n", "Fichier error");
        }
        int sequence[10] = 1;
        char seq[4];
        fseek(photo,0,SEEK_END);//将文件位置指针置于文件结尾
        int sizeoffile = ftell(photo);
        char payload[1020];
        fseek(photo,0,SEEK_SET);//将文件位置指针置于文件开头
        int times = ceil(sizeoffile/1020);


        while(sequence<=times){
            bzero(buffer,sizeof(buffer));  
            bzero(seq,sizeof(seq));  
            bzero(payload,sizeof(payload));  
            fread(payload, 1, sizeof(payload), photo);
            //printf("%s\n", payload);
            sprintf(seq, "%d\n", sequence); // int to string 
            memcpy(buffer, seq, 4);

            for(int i=0; i<1020; i++){
                buffer[4+i] = payload[i];
            }
            //struct timeval t1,t2;
            if(sequence != 10){
                sendto(select_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
            }
            sequence += 1;
            // gettimeofday(&t1,NULL);

            RETRANX:
            FD_ZERO(&fds);
            FD_SET(select_socket,&fds); //添加描述符       
            FD_SET(new_port_socket,&fds); //添加描述符 
            timeout.tv_sec = 0;
            timeout.tv_usec = 100;
            int ret = select(new_port_socket+1, &fds, NULL, NULL, &timeout);  // 要在boucle 中重新定义 timeout和fd_set因为 每次select 都会改变这些值
            if (ret < 0) {
                printf("select error!!!\n");
            }else if(ret == 0){
                printf("select timeout!!!\n");
                sendto(select_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
                goto RETRANX;
            }else{ 
                bzero(buffer,sizeof(buffer));
                if(FD_ISSET(select_socket, &fds)){
                    recvfrom(select_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len_client_addr); 
                    // gettimeofday(&t2,NULL);
                    // printf("%ld\n", t2.tv_usec - t1.tv_usec);   
                    printf("%s\n", buffer);              
                }            
            }           
        }
        break;
    }
}
