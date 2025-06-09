#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <ctype.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 45107
#define MAX_INPUT 200

// from beej socket programming section 6.2
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void error(const char *msg){
    perror(msg);
    exit(1);
}

void prompt_credentials(char *username, char *password){
    printf("Please enter the username: ");
    scanf("%s", username);
    printf("Please enter the password: ");
    scanf("%s", password);
}

void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void clear_tcp_buffer(int sockfd) {
    char dump;
    // clear TCP buffer
    while (recv(sockfd, &dump, 1, MSG_DONTWAIT) > 0);
}

int main(){
    char username[MAX_INPUT], password[MAX_INPUT];

    printf("[Client] Logging in.\n");

    int sockfd;
    struct sockaddr_in serv_addr;

    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        error("ERROR opening socket");
    }

    // Set up server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0){
        error("ERROR invalid server IP");
    }

    // Connect to ServerM
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        error("ERROR connecting to server");
    }

    // printf("[Client] Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);

    // --------------Client Login Loop -----------------
    while (1){
        prompt_credentials(username, password);

        // Send username and password to server
        char message[MAX_INPUT];
        snprintf(message, sizeof(message), "LOGIN,%s,%s", username, password);
        // sent format is username,password

        if (send(sockfd, message, strlen(message), 0) < 0){
            error("ERROR sending message to server");
        }
        printf("[Client] Sent username and password to server.\n");

        // Receive authentication result from server
        char response[20];
        int bytes_received = recv(sockfd, response, sizeof(response)-1, 0);
        if (bytes_received < 0){
            error("ERROR receiving response from server");
        }
        response[bytes_received] = '\0'; 

        if (strcmp(response, "LOGIN_SUCCESS") == 0){
            printf("[Client] You have been granted access.\n");
            break; // Exit the loop 
        } else {
            printf("[Client] The credentials are incorrect. Please try again.\n");
        }
    } // ----------------- end client login loop-----------------------
    int c;
    while ((c = getchar()) != '\n' && c != EOF); // clear stdin buffer

    //-----------------prompt user for other actions -----------------
    while (1){
        printf("[Client] Please enter the command:");
        printf("\n<quote>\n<quote <stock name>>\n<buy <stock name><number of shares>>\n<sell <stock name><number of shares>>\n<position>\n<exit>\n");
        char input[MAX_INPUT];
        // clear input buffer
        memset(input, 0, sizeof(input));
        fgets(input, sizeof(input), stdin);

        char stock_name[6] = "";
 
        input[strcspn(input,"\r\n")] = '\0';
        //to_lowercase(input);

        if (strlen(input) == 0) { continue; }

        char message[MAX_INPUT];

        // quote
        if (strcmp(input, "quote") == 0){
            strcpy(message, "QUOTE_ALL");
            //printf("QUOTE_ALL\n");
        }

        // quote stock_name
        else if (strncmp(input, "quote ", 6) == 0){
            char *stock = input + 6;
            snprintf(stock_name, sizeof(stock_name), "%s", stock);
            snprintf(message, sizeof(message), "QUOTE,%s", stock);
            //printf("QUOTE,%s\n", stock);
        }

        // position
        else if (strncmp(input, "position", 8) == 0){
            strcpy(message, "POSITION");
        }

        // sell
        else if (strncmp(input, "sell", 4) == 0){
            char stock[10];
            int shares;

            if (sscanf(input + 5, "%s %d", stock, &shares) != 2){
                printf("[Client] Error: stock name/shares are required. Please specify a stock name to buy.\n");
                continue;
            }
            
            // store stock_name
            snprintf(stock_name, sizeof(stock_name), "%s", stock);

            snprintf(message, sizeof(message), "SELL %s %d", stock, shares);
        }

        // buy
        else if (strncmp(input, "buy", 3) == 0){
            char stock[10];
            int shares;
            if (sscanf(input + 4, "%s %d", stock, &shares) != 2 || shares <= 0) {
                printf("[Client] Error: stock name/shares are required. Please specify a stock name to buy.\n");
                continue;
            }
        
            snprintf(message, sizeof(message), "BUY %s %d", stock, shares);

        }

        else if (strncmp(input, "exit",4) == 0){
            return 0;
        }

        else {
            printf("Invalid command.\n");
            continue;
        }

        // Send message QUOTE BUY SELL POSITION to server M
        if (send(sockfd, message, strlen(message), 0) < 0){
            error("ERROR sending message to serverM");
        } else {
            if (strcmp(input, "quote") == 0 || strncmp(input, "quote ", 6) == 0){
                printf("[Client] Sent a quote request to the main server.\n");
            } 
            else if (strncmp(input, "position", 8) == 0){
                printf("[Client] %s sent a position request to the main server.\n", username);
            }
        }


        // Receive response from server M
        char response[MAX_INPUT];
        memset(response, 0, sizeof(response));        
        clear_tcp_buffer(sockfd); 
        int bytes_received = recv(sockfd, response, sizeof(response)-1, 0);
        if (bytes_received < 0){
            error("ERROR receiving response from serverM");
        } 
        response[bytes_received] = '\0'; 

        if (strncmp(response, "CONFIRM_SELL", 12) == 0){
            float price = 0.0;
            sscanf(response, "CONFIRM_SELL,%f", &price); // extract price
            printf("[Client] %s's current price is %.2f Proceed to sell? (Y/N)", stock_name, price);
            
            // user enter Y or N
            char client_confirm[3]; // takes in N\n\0
            memset(client_confirm,0, sizeof (client_confirm));
            fgets(client_confirm, sizeof(client_confirm), stdin);
            client_confirm[strcspn(client_confirm, "\r\n")] = '\0'; // remove newline char


            // send Y/N to server M
            send(sockfd, client_confirm, sizeof(client_confirm), 0);

            // if client enter N, skip to next request iteration
            if (strcmp(client_confirm, "N") == 0){
                printf("__Start a new request__\n");
                memset(response, 0, sizeof(response)); 
                continue;
            }

            // wait for response from server M after confirming Y
            memset(response, 0, sizeof(response));
            int bytes_received = recv(sockfd, response, sizeof(response)-1, 0);
            response[bytes_received] = '\0';
            clear_tcp_buffer(sockfd); 

            //After client sending Y to sell
            // it can receive: 1) SELL_SUCCESS, 2) SELL_FAIL, 3) stock not found
            if (strncmp(response, "stock not found", 16) == 0){
                printf("[Client] Error: stock name does not exist. Please check again.\n");
                continue;
            }
            else if (strncmp(response, "SELL_SUCCESS", 12) == 0){
                char user[50], stock[10];
                int shares = 0;
                sscanf(response, "SELL_SUCCESS,%[^,],%[^,],%d", user, stock, &shares);
                printf("[Client] %s successfully sold %d shares of %s.\n", user, shares, stock);
                printf("--Start a new request--\n");
                continue;
            }
            else if (strncmp(response, "SELL_FAIL,",10)== 0){
                //skip SELL_FAIL,
                char *error_msg = response + 10;
                printf("%s", error_msg);
                continue;
            }
    
        } 
        else if (strncmp(response, "BUY_CONFIRM", 11) == 0){
            char stock[10]; 
            int shares;
            float price;
        
            sscanf(response, "BUY_CONFIRM,%[^,],%d,%f", stock, &shares, &price);
            //printf("[289] response: %s\n", response);
            printf("[Client] %s's current price is %.2f. Proceed to buy? (Y/N)", stock, price);

            // user enter Y or N
            char buy_confirm[3]; // takes in N\n\0
            memset(buy_confirm,0, sizeof (buy_confirm));
            fgets(buy_confirm, sizeof(buy_confirm), stdin);
            buy_confirm[strcspn(buy_confirm, "\r\n")] = '\0'; 
            
            // send buy confirm to M. Y or N
            send(sockfd, buy_confirm, strlen(buy_confirm), 0);
            
            if (strcmp(buy_confirm, "N") == 0){
                printf("__Start a new request__\n");
                memset(response, 0, sizeof(response)); 
                continue;
            }

            // receive from M result
            int bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
            if (bytes_received < 0){
                error("ERROR receiving response from serverM");
            } 
            response[bytes_received] = '\0';
            clear_tcp_buffer(sockfd); 

            char user[50];
        
            sscanf(response, "BUY_SUCCESS,%[^,],%[^,],%d", user, stock, &shares);

            struct sockaddr_in my_addr;
            socklen_t addrlen = sizeof(my_addr);

            int getsock_check = getsockname(sockfd, (struct sockaddr*)&my_addr, (socklen_t*)&addrlen);
            if (getsock_check == -1){
                perror("getsockname");
                exit(1);
            }
            printf("[Client] Received the response from the main server using TCP over port %d.\n", my_addr.sin_port);
            printf("[Client] Successfully bought %d shares of %s.\n", shares, stock);
            printf("__Start a new request__\n");
            continue;

        }
        else if (strncmp(response, "stock not found", 16) == 0){
            printf("[Client] Error: stock name does not exist. Please check again.\n");
            printf("--Start a new request--\n");
            continue;
        }
        else if (strncmp(response, "SELL_SUCCESS", 12) == 0){
            char user[50], stock[10];
            int shares = 0;
            sscanf(response, "SELL_SUCCESS,%[^,],%[^,],%d", user, stock, &shares);
            printf("[Client] %s successfully sold %d shares of %s.\n", user, shares, stock);
        }
        else if (strncmp(response, "SELL_FAIL,",10)== 0){
            //skip SELL_FAIL,
            char *error_msg = response + 10;
            printf("%s", error_msg);
            printf("--Start a new request--\n");
            continue;
        }

        // Replace comma with space
        for (int i = 0; response[i]; i++) {
            if (response[i] == ',') {
                response[i] = ' ';
            }
        }
        struct sockaddr_in my_addr;
        socklen_t addrlen = sizeof(my_addr);

        int getsock_check = getsockname(sockfd, (struct sockaddr*)&my_addr, (socklen_t*)&addrlen);
        if (getsock_check == -1){
            perror("getsockname");
            exit(1);
        }
        printf("[Client] Received the response from the main server using TCP over port %d.\n", my_addr.sin_port);
 
        if (strstr(response, "ERROR") != NULL) {
            printf("%s does not exist. Please try again.\n", stock_name);
        } 
        else if (strstr(response, "current profit") != NULL){
            printf("stock shares avg_buy_price\n");
            printf("%s", response);
        }
        else {
            printf("%s", response);
        }
        printf("--Start a new request--\n");
    }
    close(sockfd);
    return 0;
}

