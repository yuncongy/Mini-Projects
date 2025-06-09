// members.txt stores username + encrypted password

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define UDP_PORT "41107"
#define MAX_USERNAME_LENGTH 100 // contain \0\n
#define MAX_PASSWORD_LENGTH 100
#define MAX_USER 200
#define MAXBUFLEN 1024

// User info struct
typedef struct{
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
} User;

User users[MAX_USER];
int user_count = 0;

// Function Prototypes
void get_users();
void print_users(User users[], int user_count);
void toLowerCase(char* username);

// Code resued from Beej's guide
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(){
    get_users();
    //print_users(users, user_count);

    // Create UDP socket
        // Code resued from Beej's guide
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
  
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; 
  
    if ((rv = getaddrinfo(NULL, UDP_PORT, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }
  
    for(p = servinfo; p != NULL; p = p->ai_next) {
  
      if ((sockfd = socket(p->ai_family, p->ai_socktype,
           p->ai_protocol)) == -1) {
        perror("listener: socket");
        continue;
      }
  
      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        close(sockfd);
        perror("listener: bind");
        continue;
      }
  
      break;
    }
  
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    // Bind successful, print message to terminal
    printf("[Server A] Booting up using UDP on port %s.\n", UDP_PORT);

    freeaddrinfo(servinfo);
    
    addr_len = sizeof(their_addr);
    while (1){ // code from beej's guide
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        buf[numbytes] = '\0';
        buf[strcspn(buf,"\r\n")] = '\0';
        //printf("[105] received: %s\n", buf);

        //get username and password from received info
        char *username = strtok(buf, ",");
        char *encrypted_password = strtok(NULL,",");
        username[strcspn(username, "\r\n")] = '\0';
   
        printf("[Server A] Received username %s and password ******.\n", username);

        // authenticate user
        int authenticated = 0;
        char username_lc[MAX_USERNAME_LENGTH];
        strncpy(username_lc, username, MAX_USERNAME_LENGTH);
        toLowerCase(username_lc); //change all char to lowercase

        for (int i = 0; i < user_count; i++){
            char auth_username[MAX_USERNAME_LENGTH];
            strncpy(auth_username, users[i].username, MAX_USERNAME_LENGTH);
            toLowerCase(auth_username);

            // compare password
            if (strcmp(auth_username, username_lc) == 0){
                if (strcmp(users[i].password, encrypted_password) == 0){
                    authenticated = 1; // successful authentication
                }
                break;
            }
        }
        
        // Print authentication result message
        if (authenticated) {
            printf("[Server A] Member %s has been authenticated.\n", username);
        } else {
            printf("[Server A] The username %s or password ****** is incorrect.\n", username);
        }

        // send result back to server M
        char response[2];
        if (authenticated){ strcpy(response, "1"); } 
            else{ strcpy(response, "0"); }

        if ((numbytes = sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("sendto");
            continue;
        }
    }   
    close(sockfd);
    return 0;
}


void get_users(){
    FILE *file = fopen("./members.txt","r");
    if (file == NULL){
        perror("Error opening file members.txt");
        exit(1);
    }

    // reading file and store its input in users[]
    while (fscanf(file, "%s %s", users[user_count].username, users[user_count].password) == 2) {
        user_count++;
        if (user_count >= MAX_USER){
            break;
        }
    }
    fclose(file);
}

void toLowerCase(char* username){
    for (int i = 0; username[i]; i++){
        username[i] = tolower(username[i]);
    }
}

// helper function for printing file input n users[]
void print_users(User users[], int user_count) {
    printf("\n[Server A] Stored Users:\n");
    for (int i = 0; i < user_count; i++) {
        printf("  Username: %s, Password: %s\n", users[i].username, users[i].password);
    }
    printf("\n");
}
