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

#define TCP_PORT "45107" // TCP port of server M
#define UDP_PORT "44107" // UDP port of server M
#define SERVER_A_PORT 41107 // UDP connection to server A
#define SERVER_P_PORT 42107 // UDP connection to server P
#define SERVER_Q_PORT 43107 // UDP connection to server Q
#define HOST "127.0.0.1" 
#define BACKLOG 10             
#define MAXBUFLEN 1024
#define MAX_CLIENTS 10  


// from Beej's guide
void sigchld_handler(int s)
{
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


// function prototypes
void encrypt_password(char *password);
int authenticate_user(int client_fd, char *username, char *password);
void handle_client_login(int client_fd, char *username_stored);
void handle_quote(int client_fd, char *stock);
void handle_position(int client_fd, char*username);
void handle_buy(int client_fd, const char *username, const char *input);
void handle_sell(int client_fd, const char *username, char *input);
void send_UDP_message_A(struct sockaddr_in *server_addr, char *message, char *response);
void send_UDP_message_Q(struct sockaddr_in *server_addr, char *message, char *response);
void send_UDP_message_Q_M(struct sockaddr_in *server_addr, char *message, char *response);
void send_UDP_message_Q_M_P(struct sockaddr_in *server_addr, char *message, char *response);
void send_time_forward(char *stock_name);


int udp_sock, tcp_sock;
struct sockaddr_in serverA_addr, serverP_addr, serverQ_addr;


int main(void){
    // from beej's guide
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int yes = 1;
    struct sigaction sa;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, UDP_PORT, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1){
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        udp_sock = sockfd;
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    // UDP setup for server A P Q
    memset(&serverA_addr, 0, sizeof(serverA_addr));
    serverA_addr.sin_family = AF_INET;
    serverA_addr.sin_port = htons(SERVER_A_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverA_addr.sin_addr);

    memset(&serverP_addr, 0, sizeof(serverP_addr));
    serverP_addr.sin_family = AF_INET;
    serverP_addr.sin_port = htons(SERVER_P_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverP_addr.sin_addr);

    memset(&serverQ_addr, 0, sizeof(serverQ_addr));
    serverQ_addr.sin_family = AF_INET;
    serverQ_addr.sin_port = htons(SERVER_Q_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverQ_addr.sin_addr);

    // TCP Server From Beej's guide
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, TCP_PORT, &hints, &servinfo) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    // Loop through results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo); // done with this structure

    if (p == NULL) {
        fprintf(stderr, "ServerM: failed to bind TCP socket\n");
        exit(1);
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }
    
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    tcp_sock = sockfd;

    sa.sa_handler = sigchld_handler; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }// finished TCP creation

    printf("[Server M] Booting up using UDP on port %s.\n", UDP_PORT);



    // ------------------------------------main loop--------------------------------
    while (1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(tcp_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0){
            perror("Accpet client connection failed.");
            continue;
        }

        // use fork to handle multiple clients
        pid_t pid = fork();
        if (pid < 0){
            perror("Fork failed.");
            close(client_fd);
        } else if (pid == 0){
            //child 
            close(tcp_sock);
            char username_stored[50];

            handle_client_login(client_fd, username_stored);

        // checking in coming message and direct to the right function
            // while 1
            char buffer[MAXBUFLEN];
            while (1){
                memset(buffer, 0, sizeof(buffer));
                int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0) {
                    //printf("[Server M] Client disconnected.\n");
                    break;
                }

                buffer[strcspn(buffer, "\r\n")] = 0;
                
            // grab user action
                // QUOTE_ALL
                if (strcmp(buffer, "QUOTE_ALL") == 0) {
                    printf("[Server M] Received a quote request from %s, using TCP over port %s\n", username_stored, TCP_PORT);
                    handle_quote(client_fd, NULL);  
                } 

                // QUOTE,STOCK_NAME
                else if (strncmp(buffer, "QUOTE ", 6) == 0|| strncmp(buffer, "QUOTE,", 6) == 0) {
                    char *stock = buffer + 6; 
                    printf("[Server M] Received a quote request from %s for stock %s, using TCP over port %s.\n", username_stored, stock, TCP_PORT);
                    handle_quote(client_fd, stock);
                } 

                // POSITION
                else if (strncmp(buffer, "POSITION", 8) == 0){
                    printf("[Server M] Received a position request from Member to check %s's gain using TCP over port %s.\n", username_stored, TCP_PORT);
                    handle_position(client_fd, username_stored);
                }

                // SELL STOCK SHARES
                else if (strncmp(buffer, "SELL ", 5) == 0){
                    printf("[Server M] Received a sell rquest from member %s using TCP over port %s.\n", username_stored, TCP_PORT);
                    handle_sell(client_fd, username_stored, buffer);

                }

                // BUY STOCK SHARE
                else if (strncmp(buffer, "BUY ", 4) == 0){
                    printf("[Server M] Received a buy request from member %s using TCP over port %s.\n", username_stored, TCP_PORT);
                    handle_buy(client_fd, username_stored, buffer);
                }                

                // OTHER, which should not happen, I left it here just in case 
                else {
                    char *error_msg = "Other command not set yet.\n";
                    send(client_fd, error_msg, strlen(error_msg), 0);
                }
            }
            close(client_fd);
            exit(0);
        } else {
            close(client_fd); // parent
        }
    } // end while
    return 0;
}//----------------------------------- end main--------------------------------------



// Encryptes the user password by +3, does not apply to special characters
void encrypt_password(char *password){
    for (int i = 0; password[i]; i++){
        char c = password[i];
        if (c >= 'a' && c <= 'z'){
            password[i] = 'a' + (c - 'a' + 3) % 26;}
        else if (c >= 'A' && c <= 'Z'){
            password[i] = 'A' + (c - 'A' + 3) % 26;}
        else if (c >= '0' && c <= '9'){
            password[i] = '0' + (c - '0' + 3) % 10;}
        else{
            password[i] = c;} // end else
    }// end for
}



// send authentication message to server A and receive from server A
int authenticate_user(int client_fd, char *username, char *password){
    // encrypt the password
    encrypt_password(password);

    char message[MAXBUFLEN];
    // store userinput in message[] and send it using UDP
    snprintf(message, MAXBUFLEN, "%s,%s", username, password);
    //printf("[276] send message: %s\n", message);

    char response[2];

    // send the encrypted password and username to server A for authentication
    // receive the response from server A
    send_UDP_message_A(&serverA_addr, message, response);

    return (strcmp(response, "1") == 0);
}



// Send UDP meesage to server A to authenticate usernane and password
void send_UDP_message_A(struct sockaddr_in *server_addr, char *message, char *response){
    int sent = sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (sent == -1){
        perror("Server M error: sendto");
    }else{    
        printf("[Server M] Sent the authentication request to Server A\n");}

    socklen_t addr_len = sizeof(*server_addr);
    int bytes_received = recvfrom(udp_sock, response, MAXBUFLEN - 1, 0, (struct sockaddr *)server_addr, &addr_len);

    if (bytes_received == -1){
        perror("Server M error: sendto");
    }
    printf("[Server M] Received the response from server A using UDP over %s\n", UDP_PORT);
    response[bytes_received] = '\0';
}



// Farward the message of quote to server Q
void send_UDP_message_Q(struct sockaddr_in *server_addr, char *message, char *response){
    int sent = sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (sent == -1){
        perror("Server M error: sendto");
    }else{    
        printf("[Server M] Forwarded the quote request to server Q.\n");}

    socklen_t addr_len = sizeof(*server_addr);
    int bytes_received = recvfrom(udp_sock, response, MAXBUFLEN - 1, 0, (struct sockaddr *)server_addr, &addr_len);
    if (bytes_received == -1){
        perror("Server M error: sendto");
    }
    response[bytes_received] = '\0';
}



// Send the message of quote to server Q
void send_UDP_message_Q_M(struct sockaddr_in *server_addr, char *message, char *response){
    int sent = sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (sent == -1){
        perror("Server M error: sendto");
    }else{    
        printf("[Server M] Sent the quote request to server Q.\n");}

    socklen_t addr_len = sizeof(*server_addr);
    int bytes_received = recvfrom(udp_sock, response, MAXBUFLEN - 1, 0, (struct sockaddr *)server_addr, &addr_len);
    if (bytes_received == -1){
        perror("Server M error: sendto");
    }
    response[bytes_received] = '\0';
}



// this function is for position command inquiry of all quotes to Q initiated by M
void send_UDP_message_Q_M_P(struct sockaddr_in *server_addr, char *message, char *response){
    int sent = sendto(udp_sock, message, strlen(message), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
    if (sent == -1){
        perror("Server M error: sendto");
    }

    socklen_t addr_len = sizeof(*server_addr);
    int bytes_received = recvfrom(udp_sock, response, MAXBUFLEN - 1, 0, (struct sockaddr *)server_addr, &addr_len);
    if (bytes_received == -1){
        perror("Server M error: sendto");
    }
    response[bytes_received] = '\0';
}



// Function to handle client login
void handle_client_login(int client_fd, char *username_stored){
    char buffer[MAXBUFLEN];
    char username[100], password[100];
    int authenticated = 0;

    while (1){
        memset(buffer, 0, MAXBUFLEN);
        int bytes_received = recv(client_fd, buffer, MAXBUFLEN-1, 0);
        if (bytes_received == -1){
            break; // client disconnect
        }

        strncpy(username, strtok(buffer + 6, ","), sizeof(username));
        strncpy(password, strtok(NULL, ","), sizeof(password));

        printf("[Server M] Received username: %s and password ****.\n", username);

        authenticated = authenticate_user(client_fd, username, password);

        if (authenticated) {
            send(client_fd, "LOGIN_SUCCESS",13, 0);
            break;
        }else{
            send(client_fd, "LOGIN_FAILED",12, 0);
        }

        printf("[Server M] Sent the response from server A to the client using TCP over port %s.\n", TCP_PORT);
    } // end while 1

    strncpy(username_stored, username, 50);
} 



// handle quote command 
void handle_quote(int client_fd, char *stock){
    char request[MAXBUFLEN];
    char response[MAXBUFLEN];

    if (stock == NULL){
        strcpy(request, "QUOTE_ALL");
    } else {
        snprintf(request, sizeof(request), "QUOTE,%s", stock);
    }

    send_UDP_message_Q(&serverQ_addr, request, response);

    if (stock){
        printf("[Server M] Received the quote response from server Q for stock %s using UDP over %s\n", stock, UDP_PORT);
    } else {
        printf("[Server M] Received the quote response from server Q using UDP over %s\n", UDP_PORT);
    }

    send(client_fd, response, strlen(response), 0);

    printf("[Server M] Forwarded the quote response to the client.\n");
    
}



// handle position command
void handle_position(int client_fd, char*username){
    char response_from_Q[MAXBUFLEN];
    char message_to_Q[] = "QUOTE_ALL";

    // requeest all prices from Q
    send_UDP_message_Q_M_P(&serverQ_addr, message_to_Q, response_from_Q);

    // message to P. Format: POSITION,username,stock1,price1,stock2,price2,...
    char message_to_P[1024];
    snprintf(message_to_P, sizeof(message_to_P), "POSITION,%s,%s", username, response_from_Q);

    // send to P
    sendto(udp_sock, message_to_P, strlen(message_to_P), 0, (struct sockaddr *)&serverP_addr, sizeof(serverP_addr));
    printf("[Server M] Forwarded the position request to server P.\n");

    // receive from P
    char response_from_P[1024];
    socklen_t addr_len = sizeof(serverP_addr);
    int bytes_received = recvfrom(udp_sock, response_from_P, sizeof(response_from_P) - 1, 0, (struct sockaddr *)&serverP_addr, &addr_len);
    if (bytes_received == -1) {
        perror("[Server M] Error receiving from server P");
        return;
    }

    response_from_P[bytes_received] = '\0';
    printf("[Server M] Received user's portfolio from server P using UDP over %s\n", UDP_PORT);

    // send to client
    send(client_fd, response_from_P, strlen(response_from_P), 0);
    printf("[Server M] Forwarded the gain to the client.\n");
}



// handle sell command
void handle_sell(int client_fd, const char *username, char *input){
    char stock[10];
    int shares;

    sscanf(input + 5, "%s %d", stock, &shares);

    // Ask Q for stock price
    char message_to_Q[50];
    char response_from_Q[50];
    snprintf(message_to_Q, sizeof(message_to_Q), "QUOTE,%s", stock);
    send_UDP_message_Q_M(&serverQ_addr, message_to_Q, response_from_Q);

    // check if the stock is valid fron response_from_Q
    if (strstr(response_from_Q, "ERROR") != NULL){
        // if not, send stock not found to client
        send(client_fd, "stock not found", 16, 0);
        //send_time_forward(stock);
        return;
    }

    char stock_recv[10];
    float stock_price = 0.0;
    sscanf(response_from_Q, "%[^,],%f", stock_recv, &stock_price); // format: STOCK_NAME,PRICE

    // Ask P if user has enough shares
    char message_to_P[100];
    snprintf(message_to_P, sizeof(message_to_P), "SELL,%s,%s,%d", username, stock, shares);

    sendto(udp_sock, message_to_P, strlen(message_to_P), 0, (struct sockaddr *)&serverP_addr, sizeof(serverP_addr));
    printf("[Server M] Forwarded the sell request to server P.\n");

    // receive response from P
    char response_from_P[100];
    socklen_t addr_len = sizeof(serverP_addr);
    int bytes_received = recvfrom(udp_sock, response_from_P, sizeof(response_from_P) - 1, 0, (struct sockaddr *)&serverP_addr, &addr_len);
    if(bytes_received == -1) {
        perror("[Server M] Error receiving from server P");
        return;
    }
    response_from_P[bytes_received] = '\0';

    // checking response from P, see if it asks user to confirm sell , or user does not have enough shares
    if (strncmp(response_from_P, "CONFIRM_SELL",12) == 0){
        char confirm_msg[20]; // contains CONFIRM_SELL, price
        memset(confirm_msg, 0, sizeof(confirm_msg)); 
        snprintf(confirm_msg, sizeof(confirm_msg), "CONFIRM_SELL,%.2f",stock_price);

        // send CONFIRM_SELL, price to client
        send(client_fd, confirm_msg, strlen(confirm_msg),0);
        printf("[Server M] Farwarded the sell confirmation to the client.\n");
        
        char client_reply[2]; // Y / N
        int bytes_received = recv(client_fd, client_reply, sizeof(client_reply) - 1, 0);
        if (bytes_received <= 0) {
            printf("[Server M] Client disconnected.\n");
            return;
        }
        client_reply[bytes_received] = '\0';
        client_reply[strcspn(client_reply, "\r\n")] = 0;

        // check client reply Y or N
        if (strcmp(client_reply, "Y") == 0){

            // forward CONFIRM_SELL to P
            sendto(udp_sock, response_from_P, strlen(response_from_P), 0, (struct sockaddr *)&serverP_addr, sizeof(serverP_addr));
            printf("[Server M] Forwarded the sell confirmation response to Server P.\n");

            int bytes_received = recvfrom(udp_sock, response_from_P, sizeof(response_from_P) - 1, 0, (struct sockaddr *)&serverP_addr, &addr_len);
            if (bytes_received == -1) {
                perror("[Server M] Error receiving from server P");
                return;
            }
            response_from_P[bytes_received] = '\0';

            // format "SELL_SUCCESS, %s, %s, %d", username, stock, shares
            char success_msg[100];
            snprintf(success_msg, sizeof(success_msg), "[Client] %s successfully sold %d number of %s.\n", username, shares, stock);
            send(client_fd, response_from_P, strlen(response_from_P), 0);
            printf("[Server M] Forwarded the sell result to the client.\n");
            send_time_forward(stock);
        } else { // client sends N

            // farward DENIED_SELL to P
            char *denied_message = "DENIED_SELL";
            sendto(udp_sock, denied_message, strlen(denied_message), 0, (struct sockaddr *)&serverP_addr, sizeof(serverP_addr));
            printf("[Server M] Forwarded the sell confirmation response to Server P.\n");
            send_time_forward(stock);

        }// end if else
    }// end if confirm sell
    else if (strncmp(response_from_P, "SELL_FAIL", 9) == 0){
        // format SELL_FAIL, %s, %s, %d", username, stock, shares
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "SELL_FAIL,[Client] Error: %s does not have enough shares of %s to sell. Please try again.\n", username, stock);
        printf("[Server M] Forwarded the sell result to the client.\n");
        send(client_fd, error_msg, strlen(error_msg), 0);
        send_time_forward(stock);
    }
}



// handle buy command
void handle_buy(int client_fd, const char *username, const char *input){
    char stock[10];
    int shares;

    // Parse input: BUY S1 10
    sscanf(input + 4, "%s %d", stock, &shares);

    // Ask Q for stock price
    char message_to_Q[50], response_from_Q[50];
    snprintf(message_to_Q, sizeof(message_to_Q), "QUOTE,%s", stock);
    send_UDP_message_Q_M(&serverQ_addr, message_to_Q, response_from_Q);

    if (strstr(response_from_Q, "ERROR") != NULL){
        send(client_fd, "stock not found", 16, 0);
        return;
    }

    float price = 0.0;
    sscanf(response_from_Q, "%*[^,],%f", &price);  // skip stock name

    // Send BUY_CONFIRM to client
    char confirm_msg[100];
    snprintf(confirm_msg, sizeof(confirm_msg), "BUY_CONFIRM,%s,%d,%.2f", stock, shares, price);
    send(client_fd, confirm_msg, strlen(confirm_msg), 0);
    printf("[Server M] Sent the buy confirmation to the client.\n");

    // Receive client confirmation
    char client_reply[10];
    int bytes_received = recv(client_fd, client_reply, sizeof(client_reply)-1, 0);
    if (bytes_received <= 0) {
        printf("[Server M] Client disconnected.\n");
        return;
    }
    client_reply[bytes_received] = '\0';
    client_reply[strcspn(client_reply, "\r\n")] = 0;

    if (strcmp(client_reply, "Y") != 0 && strcmp(client_reply, "y") != 0) { // if user answer N
        send(client_fd, "BUY_DENIED\n", 10, 0);
        printf("[Server M] Buy denied.\n");
        send_time_forward(stock);
        return;
    }
    printf("[Server M] Buy approved.\n");

    // send BUY to P
    char message_to_P[100];
    snprintf(message_to_P, sizeof(message_to_P), "BUY,%s,%s,%d,%.2f", username, stock, shares, price);
    sendto(udp_sock, message_to_P, strlen(message_to_P), 0, (struct sockaddr *)&serverP_addr, sizeof(serverP_addr));
    printf("[Server M] Forwarded the buy confirmation response to Server P.\n");

    // Receive from P
    char response_from_P[100];
    socklen_t addr_len = sizeof(serverP_addr);
    bytes_received = recvfrom(udp_sock, response_from_P, sizeof(response_from_P)-1, 0,
                              (struct sockaddr *)&serverP_addr, &addr_len);
    if (bytes_received < 0){
        perror("[Server M] Error receiving from P");
        return;
    }
    response_from_P[bytes_received] = '\0'; // format "BUY_SUCCESS,%s,%s,%d", username, stock, shares"

    // forward to client
    send(client_fd, response_from_P, strlen(response_from_P), 0);
    printf("[Server M] Forwarded the buy result to the client.\n");

    //set time forward
    send_time_forward(stock);    
}



// tell server Q to advance timer
void send_time_forward(char *stock_name){
    char time_forward_message[MAXBUFLEN];
    snprintf(time_forward_message, sizeof(time_forward_message), "TIME_ADVANCE,%s", stock_name);
    sendto(udp_sock, time_forward_message, strlen(time_forward_message), 0, (struct sockaddr *)&serverQ_addr, sizeof(serverQ_addr));
    printf("[Server M] Sent a time forward request for %s.\n", stock_name);
}