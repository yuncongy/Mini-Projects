//using quotes.txt. stores 10 historical prices per stock
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

#define UDP_PORT 43107
#define BUFFER_SIZE 100
#define MAX_QUOTES 10
#define MAX_STOCK_LENGTH 7

typedef struct{
    char name[MAX_STOCK_LENGTH];
    float prices[MAX_QUOTES];
    int current_index;  // keep track of current stock index with looping
    int total_time_index; // keep track of total time 
} Stock;

Stock stocks[MAX_QUOTES];
int stock_count = 0;

// function prototypes
void load_quotes();
int find_stock_index(const char *stock_name);

int main(){
    // partly reused from Beej's guide on UDP and serverA.c
    int sockfd;
    struct sockaddr_in serv_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    load_quotes();

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(UDP_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    printf("[Server Q] Booting up using UDP on port %d.\n", UDP_PORT);

    while (1){
        memset(buffer, 0, BUFFER_SIZE);
        if (recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len) < 0){
            perror("recvfrom");
            continue;
        }

        //Quote ALL
        if (strncmp(buffer,"QUOTE_ALL", 9)==0){
            printf("[Server Q] Received a quote request from the main server.\n");

            char reply[BUFFER_SIZE];
            memset(reply, 0, sizeof(reply));

            for (int i = 0; i < stock_count;i++){
                int ind = stocks[i].current_index; //current stock index
                float price = stocks[i].prices[ind]; // current price at stock index
                char *stock_name = stocks[i].name;

                char line[BUFFER_SIZE];
                snprintf(line, sizeof(line), "%s,%.2f\n", stock_name, price);
                strcat(reply, line);
            } //endfor

            int sent = sendto(sockfd, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, addr_len);
            if (sent == -1){
                perror("sendto");
                continue;
            }else {
                printf("[Server Q] Returned all stock quotes.\n");
            }
        }// end if  QUOTE_ALL

        else if (strncmp(buffer, "QUOTE", 5) == 0){
            //char *command = 
            strtok(buffer, ","); // QUOTE
            char *stock_name = strtok(NULL, ",");
            stock_name[strcspn(stock_name,"\r\n")] = '\0'; // remove newline char 

            printf("[Server Q] Received a quote request from the main server for stock %s.\n", stock_name);

            int ind = find_stock_index(stock_name);
            char reply[BUFFER_SIZE];

            if (ind >= 0){ // if stock is found 
                int stock_ind = stocks[ind].current_index;
                float price = stocks[ind].prices[stock_ind];

                snprintf(reply, sizeof(reply), "%s,%.2f\n", stock_name, price);
            } else {
                // reply format for stock not found: STOCK_NAME,ERROR
                snprintf(reply, sizeof(reply), "%s,ERROR", stock_name);
            }
            
            int sent = sendto(sockfd, reply, strlen(reply), 0, (struct sockaddr *)&client_addr, addr_len);
            if (sent == -1){
                perror("sendto");
                continue;
            } else {
                printf("[Server Q] Returned the stock quote of %s.\n", stock_name);
            }
        }// end else if QUOTE

        else if (strncmp(buffer, "TIME_ADVANCE", 12) == 0){
            //char *command = 
            strtok(buffer, ",");
            char *stock_name = strtok(NULL, ",");
            stock_name[strcspn(stock_name,"\r\n")] = '\0';

            printf("[Server Q] Received a time forward request for %s, ", stock_name);
            
            int ind = find_stock_index(stock_name);
            int stock_ind = stocks[ind].current_index;

            // advance to next price
            stocks[ind].current_index = (stock_ind +1) % MAX_QUOTES; // increase stock index and loop
            stocks[ind].total_time_index++; 
            float current_price = stocks[ind].prices[stocks[ind].current_index]; // get the new price
            printf("the current price of that stock is %.2f at time %d.\n", current_price, stocks[ind].total_time_index);
        }// end else if 
    } // end while 1
    return 0;
}



// load quotes from quote.txt and store it
void load_quotes(){
    FILE *file = fopen("./quotes.txt","r");
    if (!file){
        perror("ERROR opening quotes.txt");
        exit(1);
    }

    while (fscanf(file, "%s", stocks[stock_count].name) == 1){
        for (int i = 0; i < MAX_QUOTES; i++){
            fscanf(file, "%f", &stocks[stock_count].prices[i]);
        }
        stocks[stock_count].current_index = 0;
        stock_count++;
    }
    fclose(file);
}



// given the name find the index of stock in data structure
int find_stock_index(const char *stock_name){
    for (int i = 0; i < stock_count; i++){
        if (strcmp(stocks[i].name, stock_name) == 0){
            return i;
        }
    }
    return -1; // means stock not found
}