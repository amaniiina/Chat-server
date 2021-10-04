#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_BUFF_LEN 1024

void start_listening(int max_clients, int main_socketfd);

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("Usage: chatserver <port> <max_clients>");
        exit(0);
    }
    int port= atoi(argv[1]);
    int max_clients= atoi(argv[2]);
    if(port<=0 || max_clients<=0){
        printf("Usage: chatserver <port> <max_clients>");
        exit(0);
    }

    int main_socketfd= socket(AF_INET, SOCK_STREAM, 0);    
    if(main_socketfd < 0){
        perror("socket\n");
        exit(1);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family= AF_INET;
    serv_addr.sin_addr.s_addr= htonl(INADDR_ANY);
    serv_addr.sin_port= htons(port);

    if(bind(main_socketfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        perror("bind\n");
        close(main_socketfd);
        exit(1);
    }

    start_listening(max_clients, main_socketfd); 
   
    close(main_socketfd);
    return 0;
}

void start_listening(int max_clients, int main_socketfd){

    int fd_arr[max_clients + 1];
    memset(fd_arr,0, sizeof(int)*(max_clients+1));

    fd_arr[0]= main_socketfd;

    fd_set read_set, write_set;
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_SET(main_socketfd, &read_set);

    char buffer[MAX_BUFF_LEN];
    memset(buffer, 0, MAX_BUFF_LEN+1);
    int new_fd, bytes=0, client_count=0;

    while(1){
        if(bytes!=0){
            for(int i=1; i < max_clients+1; i++){
                    int fd= fd_arr[i]; 
                // if client is ready for write 
                if(fd > 0 && FD_ISSET(fd, &write_set)){
                    printf("fd %d is ready to write\n", fd);
                    if((bytes= write(fd, buffer, MAX_BUFF_LEN)) <0){
                        // write failed
                        perror("write\n");
                        exit(1);
                    }
                    // else successful write
                }
            }
            memset(buffer, 0, MAX_BUFF_LEN+1);
            bytes=0;
        }

        // reinitialize read and write fd_set after select
        for(int i=0; i< max_clients+1; i++){
            if(fd_arr[i]!=0){
                FD_SET(fd_arr[i], &read_set);
                if(i > 0){
                    FD_SET(fd_arr[i], &write_set);
                }
            }
        }
        
        // select on read fd set and write fd set
        if(select(getdtablesize(), &read_set, &write_set, NULL, NULL) <0 ){
            perror("select\n");
            exit(1);
        }
        // check main socket (welcome socket)
        if(FD_ISSET(main_socketfd, &read_set)){
            if(client_count < max_clients){
                listen(main_socketfd, 2);
                new_fd= accept(main_socketfd, NULL, NULL);
                if(new_fd<0){
                    perror("accept\n"); 
                    exit(1);
                }else{
                    // add client in fd_arr and set it in read and write fd sets
                    client_count++;
                    for(int i=1; i<max_clients+1; i++){
                        if(fd_arr[i]!=0){
                            continue;
                        }
                        fd_arr[i]= new_fd;
                        break;
                    }
                    FD_SET(new_fd, &read_set);
                    FD_SET(new_fd, &write_set);
                }
            }
            // else reached max clients
        }
        for(int i=1; i < max_clients+1; i++){
            int fd= fd_arr[i]; 
            // if client is ready for read
            if(fd > 0 && FD_ISSET(fd, &read_set) ){
                printf("fd %d is ready to read\n", fd);
                memset(buffer, 0, MAX_BUFF_LEN+1);
                if((bytes= read(fd, buffer, MAX_BUFF_LEN)) <0){
                    // read failed
                    perror("read\n");
                    exit(1);
                }
                // if client closed connection
                else if(bytes==0){
                    close(fd);
                    client_count--;
                    FD_CLR(fd, &read_set);
                    FD_CLR(fd, &write_set);
                    for(int k=0; k < max_clients+1; k++){
                        if(fd_arr[k]==fd){
                            fd_arr[k]=0;
                            break;
                        }
                    }
                }
                // else successful read now we can write
            }
        }
    }
}