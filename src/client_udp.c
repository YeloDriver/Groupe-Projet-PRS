#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

int main(int argc, char* argv[]){
    int domaine = AF_INET;
    int type = SOCK_DGRAM;
    int protocle = 0;
    int err;
    char buffer[1024];

    int new_socket = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in client_addr;
    memset((char*)&client_addr, 0, sizeof(client_addr));
    
    client_addr.sin_family = domaine;
    client_addr.sin_port = htons(atoi(argv[1]));
    // faciliter initialisation du champ s_addr
    err = inet_aton("127.0.0.1", &client_addr.sin_addr);
    socklen_t len_client_addr = sizeof(client_addr);
    
    // Etablir tcp

    sendto(new_socket, "SYN", strlen("SYN"), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
    recvfrom(new_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len_client_addr);
    printf("%s\n", buffer);

    if (strcmp(buffer, "ACK-SYN")!=0){
        printf("%s\n", "Connect error");
    }else{
        sendto(new_socket, "ACK", strlen("ACK"), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        printf("%s\n", "Connect etablit");
    }
    bzero(buffer, sizeof(buffer));

    // Ouverture la nouvelle port 
    recvfrom(new_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len_client_addr);
    int new_client_socket = socket(domaine, type, protocle);      
    struct sockaddr_in new_client_addr;
    memset((char*)&new_client_addr, 0, sizeof(new_client_addr));
    new_client_addr.sin_family = domaine;
    new_client_addr.sin_port = htons(atoi(buffer));
    printf("%s\n", buffer);
    err = inet_aton("127.0.0.1", &new_client_addr.sin_addr);
    socklen_t new_len_client_addr = sizeof(new_client_addr);
    //sendto(new_client_socket, "Hello", strlen("Hello"), 0, (struct sockaddr*)&new_client_addr, sizeof(new_client_addr));
    sendto(new_client_socket, "Hello", strlen("Hello"), 0, (struct sockaddr*)&new_client_addr, sizeof(new_client_addr));
        
    FILE* fp;
    if((fp = fopen("new1.jpg","wb") ) == NULL ){
        printf("File.\n");
        exit(1);
    }
    int n;
    char msg[1020];
    char seq[4];
    while(1){  
        n = recvfrom(new_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &len_client_addr);     
        if(n == 0){
            break;
        }else{
            memcpy(seq, buffer, 4);
            for(int i=0; i<1020; i++){
                msg[i] = buffer[4+i];
            }  
            char ack[10];
            sprintf(ack, "%s%s\n", "ACK_", seq);
            fwrite(msg, 1, sizeof(msg), fp);
            bzero(buffer, sizeof(buffer));
            sendto(new_socket, ack, sizeof(ack), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        }
    }
}