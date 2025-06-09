// use portfolios.txt stores stock holdings per user
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

#define MAX_USERS 200
#define MAX_USERNAME_LENGTH 100
#define MAX_PASSWORD_LENGTH 100
#define MAX_STOCK_NAME_LENGTH 7
#define MAX_STOCKS 100
#define UDP_PORT 42107
#define BUFFER_SIZE 1024 // big buffer for sending enough stock info data

typedef struct{
    char stock[MAX_STOCK_NAME_LENGTH];
    int shares;
    float avg_price;
} StockHolding;

typedef struct{
    char username[MAX_USERNAME_LENGTH];
    StockHolding holdings[MAX_STOCKS];
    int stock_count;
} Portfolio;

typedef struct{
    char stock_name[MAX_STOCK_NAME_LENGTH];
    float current_price;
} AvgStockPrice; // stores the incoming stock price server M got from server Q

AvgStockPrice avg_stock_prices[MAX_STOCKS];
int stock_price_count = 0; // stores the number of stock prices received from M

Portfolio portfolios[MAX_USERS];
int portfolio_count = 0;



//function prototypes
void load_portfolios();
void test_load_portfolios();
void parse_M_stock_data(const char *buffer);
float calculate_user_profit(const char *username);
void handle_confirm_sell_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd);
void handle_sell_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd);
void handle_position_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd);
void handle_buy_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd);


int main(){
    // partly reused from Beej's guide on UDP and serverA.c
    int sockfd;
    struct sockaddr_in serv_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);
    //char *username = NULL;

    load_portfolios();
    //test_load_portfolios();

    memset(&serv_addr, 0, sizeof(serv_addr)); 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(UDP_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0){
        perror("bind failed.\n");
        exit(1);
    };
    

    printf("[Server P] Booting up using UDP on port %d.\n", UDP_PORT);

    while (1){
        memset(buffer, 0, BUFFER_SIZE);
        //printf("DEBUG: [Server P] waiting on receive\n");
        if (recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len) < 0){
            perror("recvfrom");
            continue;
        }
        //debug print
        //printf("DEBUG: Received message %s\n", buffer);

        if (strncmp(buffer, "POSITION,", 9) == 0) {
            /*
            username = buffer + 9; // Skip "POSITION,"
            username = strtok(username, ",");
            printf("[Server P] Received a position request from the main server for Member: %s\n", username);
            */
           handle_position_command(buffer, &client_addr, addr_len, sockfd);
        }
        else if (strncmp(buffer, "SELL,", 5) == 0){
            handle_sell_command(buffer, &client_addr, addr_len, sockfd);
        }
        else if (strncmp(buffer, "CONFIRM_SELL", 12) == 0){
            printf("[Server P] User approves selling the stock.\n");
            handle_confirm_sell_command(buffer, &client_addr, addr_len, sockfd);
        }
        else if (strncmp(buffer, "DENIED_SELL", 11) == 0){
            printf("[Server P] Sell denied.\n");
        }
        else if (strncmp(buffer, "BUY,", 4) == 0) {
            handle_buy_command(buffer, &client_addr, addr_len, sockfd);
        }
        else{
            printf("Not able to parse message: %s\n", buffer);
        }
    }// end while
    return 0;
} // end main



// get the number of shares of a stock for a user
// incoming format from M: SHARE,username,stock_name
int get_user_shares(const char *username, const char *stock) {
    printf("[154] Finding user shares: %s for stock %s\n", username, stock);
    for (int i = 0; i < portfolio_count; i++) {
        if (strcmp(portfolios[i].username, username) == 0) {
            for (int j = 0; j < portfolios[i].stock_count; j++) {
                if (strcmp(portfolios[i].holdings[j].stock, stock) == 0) {
                    return portfolios[i].holdings[j].shares;
                }
            }
        }
    }
    return -1;  // stock not found or user not found
}



// After user send Y to sell, update the user's portfolio
// incoming format from M: SELL,username,stoc,shares
int confirm_sell(const char *username, const char *stock, int shares_to_sell) {
    for (int i = 0; i < portfolio_count; i++) {
        if (strcmp(portfolios[i].username, username) == 0) {
            for (int j = 0; j < portfolios[i].stock_count; j++) {
                if (strcmp(portfolios[i].holdings[j].stock, stock) == 0) {
                    if (portfolios[i].holdings[j].shares >= shares_to_sell) {
                        portfolios[i].holdings[j].shares -= shares_to_sell;
                        return 1; // success
                    } else {
                        return 0; // not enough shares
                    }
                }
            }
        }
    }
    return -1; // user or stock not found
}



// handle sell 
void handle_sell_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd) {
    char temp[BUFFER_SIZE];
    strncpy(temp, buffer, BUFFER_SIZE);

    char *token = strtok(temp, ",");

    token = strtok(NULL, ",");
    if (!token) return;
    char username[MAX_USERNAME_LENGTH];
    strncpy(username, token, MAX_USERNAME_LENGTH);
    //change user name to lowercase for consistent storage
    for (int i = 0; username[i]; i++){
        username[i] = tolower(username[i]);
    }

    token = strtok(NULL, ",");       // stock
    if (!token) return;
    char stock[MAX_STOCK_NAME_LENGTH];
    strncpy(stock, token, MAX_STOCK_NAME_LENGTH);

    token = strtok(NULL, ",");       // shares
    if (!token) return;
    int shares = atoi(token);

    printf("[Server P] Received a sell request from the main server.\n");

    int current_shares = get_user_shares(username, stock);
    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));

    if (current_shares >= shares){
        // if enough shares, ask M to confirm sell from user
        snprintf(response, sizeof(response), "CONFIRM_SELL,%s,%s,%d", username, stock, shares);
        printf("[Server P] Stock %s has sufficient shares in %s's portfolio. Requesting users' confirmation for selling stock.\n", stock, username);
    }
    else{
        // not enough shares, send SELL_FAIL to M
        snprintf(response, sizeof(response), "SELL_FAIL,%s,%s,%d", username, stock, shares);
        printf("[Server P] Stock %s does not have enough shares in %s's portfolio. Unable to sell %d shares of %s.\n", stock, username, shares, stock);
    }
    sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)client_addr, addr_len);
    
}// end handle_sell_command



// handle sell after user sends confrimation(Y)
void handle_confirm_sell_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd) {
    char temp[BUFFER_SIZE];
    strncpy(temp, buffer, BUFFER_SIZE);
    //printf("CONFIRM SELL COMMAND FUNC: %s\n", temp);
    char *token = strtok(temp, ","); // CONFIRM_SELL
    token = strtok(NULL, ",");       // username
    //printf("TOKEN: (SHOULD BE USERNAME) %s\n", token);
    if (!token) return;
    char username[MAX_USERNAME_LENGTH];
    strncpy(username, token, MAX_USERNAME_LENGTH);
    //change user name to lowercase
    for (int i = 0; username[i]; i++){
        username[i] = tolower(username[i]);
    }    

    token = strtok(NULL, ",");       // stock
    if (!token) return;
    char stock[MAX_STOCK_NAME_LENGTH];
    strncpy(stock, token, MAX_STOCK_NAME_LENGTH);

    token = strtok(NULL, ",");       // shares
    if (!token) return;
    int shares = atoi(token);

    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));
    if (confirm_sell(username, stock, shares) != -1){

        // if success, send "SELL_SUCCESS, username, stock, shares" to M
        snprintf(response, sizeof(response), "SELL_SUCCESS, %s, %s, %d", username, stock, shares);
        printf("[Server P] Successfully sold %d shares of %s and updated %s's portfolio.\n", shares, stock, username);
    } else {

        // if failed, which should not happen because we checked the amout of shares before
        snprintf(response, sizeof(response), "SELL_FAIL, %s, %s, %d", username, stock, shares);
        printf("[WARNING] should not happen"); // debug
    }

    sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)client_addr, addr_len);
}// end handle_confirm_sell_command



// handle buy command
void handle_buy_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd) {
    char temp[BUFFER_SIZE];
    strncpy(temp, buffer, BUFFER_SIZE);

    strtok(temp, ","); // BUY
    char *username = strtok(NULL, ",");
    //change user name to lowercase
    for (int i = 0; username[i]; i++){
        username[i] = tolower(username[i]);
    }
    char *stock = strtok(NULL, ",");
    char *shares_str = strtok(NULL, ",");
    char *price_str = strtok(NULL, ",");

    if (!username || !stock || !shares_str || !price_str){
        printf("Missing info.\n");
        return;
    }
    int shares = atoi(shares_str);
    float price = atof(price_str);

    printf("[Server P] Received a buy request from the client.\n");

    for (int i = 0; i < portfolio_count; i++) {
        if (strcmp(portfolios[i].username, username) == 0) { // find username
            int found = 0; 

            for (int j = 0; j < portfolios[i].stock_count; j++) { // find stock
                if (strcmp(portfolios[i].holdings[j].stock, stock) == 0) {
                    // Update avg buy price
                    int old_shares = portfolios[i].holdings[j].shares;
                    float old_price = portfolios[i].holdings[j].avg_price;

                    float new_avg = ((old_shares * old_price) + (shares * price)) / (old_shares + shares);
                    portfolios[i].holdings[j].shares += shares;
                    portfolios[i].holdings[j].avg_price = new_avg;
                    
                    found = 1;
                    break;
                }
            }// end inner for

            if (!found) {
                // Add new stock
                StockHolding *h = &portfolios[i].holdings[portfolios[i].stock_count++];
                strncpy(h->stock, stock, MAX_STOCK_NAME_LENGTH);
                h->shares = shares;
                h->avg_price = price;
            }

            char response[BUFFER_SIZE];
            memset(response, 0, sizeof(response));
            // compose response to M
            snprintf(response, sizeof(response), "BUY_SUCCESS,%s,%s,%d", username, stock, shares); 
            // send response to M
            sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)client_addr, addr_len);

            printf("[Server P] Successfully bought %d shares of %s and updated %s’s portfolio.\n", shares, stock, username);
            return;
        }
    }//end for

    // user not in portfolio, create new
    Portfolio *new_user = &portfolios[portfolio_count++];
    strncpy(new_user->username, username, MAX_USERNAME_LENGTH);
    new_user->stock_count = 1;

    StockHolding *h = &new_user->holdings[0];
    strncpy(h->stock, stock, MAX_STOCK_NAME_LENGTH);
    h->shares = shares;
    h->avg_price = price;

    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));            
    // compse response to M
    snprintf(response, sizeof(response), "BUY_SUCCESS,%s,%s,%d", username, stock, shares); 
    // send response to M
    sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)client_addr, addr_len);

    printf("[Server P] Successfully bought %d shares of %s and updated %s’s portfolio.\n", shares, stock, username);
}// end handle buy command




// handle position command
void handle_position_command(const char *buffer, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd){
    // Extract username
    char *username = NULL;
    char temp_buffer[BUFFER_SIZE];
    strncpy(temp_buffer, buffer, BUFFER_SIZE);
    username = temp_buffer + 9; // Skip "POSITION,"
    username = strtok(username, ",");
    printf("[Server P] Received a position request from the main server for Member: %s\n", username);

    // Parse price info
    parse_M_stock_data(buffer); 

    // Calculate profit
    //change user name to lowercase
    for (int i = 0; username[i]; i++){
        username[i] = tolower(username[i]);
    }

    float profit = calculate_user_profit(username);

    // Build response
    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));

    for (int i = 0; i < portfolio_count; i++) {
        if (strcmp(portfolios[i].username, username) == 0) {
            for (int j = 0; j < portfolios[i].stock_count; j++) {
                StockHolding *h = &portfolios[i].holdings[j];
                char stock_info[50];
                snprintf(stock_info, sizeof(stock_info), "%s %d %.2f\n", h->stock, h->shares, h->avg_price);
                strcat(response, stock_info);
            }
            break;
        }
    }

    // Add profit info
    char profit_str[50];
    snprintf(profit_str, sizeof(profit_str), "%s's current profit is %.2f\n", username, profit);
    //printf("[380] sent profit: %.2f\n", profit);
    strcat(response, profit_str);

    // Send response back to M
    //printf("DEBUG: Sending response: %s\n", response);
    sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)client_addr, addr_len);
    printf("[Server P] Finished sending the gain and portfolio of %s to the main server.\n", username);
}



// calculates user's profit
float calculate_user_profit(const char *username){
    float total = 0.0;

    // find the user
    for (int i = 0; i < portfolio_count; i++){
        if (strcmp(portfolios[i].username, username) == 0){
            //for each stock in the user
            for (int j = 0; j < portfolios[i].stock_count; j++){
                StockHolding *h = &portfolios[i].holdings[j];

                // find current price of the stock
                for (int k = 0; k < stock_price_count; k++){
                    if (strcmp(h->stock, avg_stock_prices[k].stock_name) == 0){
                        //calculate profit
                        float profit = (avg_stock_prices[k].current_price - h->avg_price) * h->shares;
                        total += profit;
                    }
                }
            }
            return total;
        }
    }
    return 0.0; // user not found
}



// Parse price info from buffer, used by position command
void parse_M_stock_data(const char *buffer){
    stock_price_count = 0; // reset stock price count

    char buffer_copy[BUFFER_SIZE];
    strncpy(buffer_copy, buffer, BUFFER_SIZE);
    buffer_copy[BUFFER_SIZE - 1] = '\0'; 

    // Replace any newline with a comma so we can parse correctly
    for (int i = 0; buffer_copy[i]; i++) {
        if (buffer_copy[i] == '\n' || buffer[i] == '\r') {
            buffer_copy[i] = ',';  
        }
    }

    char temp_buffer[BUFFER_SIZE];
    strncpy(temp_buffer, buffer_copy, BUFFER_SIZE);

    //parse the stock data
    char *token = strtok(temp_buffer, ",");
    token = strtok(NULL, ",");
    
    while ((token  =strtok(NULL, ",")) != NULL){
        char *stock = token;
        char *price_ptr = strtok(NULL, ",");

        if (!price_ptr) break;

        // store in avg_stock_prices
        if (stock_price_count < MAX_STOCKS){
            strncpy(avg_stock_prices[stock_price_count].stock_name, stock, MAX_STOCK_NAME_LENGTH);
            avg_stock_prices[stock_price_count].current_price = atof(price_ptr);
            stock_price_count++;
        } else {
            fprintf(stderr, "Error: Exceeded maximum number of stock prices.\n");
            break;
        }
    }
}



// load porfolios from portfolios.txt
void load_portfolios() {
    FILE *file = fopen("./portfolios.txt", "r");
    if (!file) {
        perror("Error opening portfolios.txt");
        exit(1);
    }

    char line[100];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;

        // Trim trailing spaces， there are space after names in input file
        int len = strlen(line);
        while (len > 0 && isspace(line[len - 1])) {
            line[--len] = '\0';
        }

        // Skip empty lines
        if (strlen(line) == 0) continue;

        // Start new portfolio for username
        Portfolio *p = &portfolios[portfolio_count++];
        
        //change user name to lowercase
        for (int i = 0; line[i]; i++){
            line[i] = tolower(line[i]);
        }

        strncpy(p->username, line, MAX_USERNAME_LENGTH);
        p->stock_count = 0;

        // Read 
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\r\n")] = 0;
            if (strlen(line) == 0) continue;

            // Check if line is a stock or a new username
            char stock[MAX_STOCK_NAME_LENGTH];
            int shares;
            float price;

            if (sscanf(line, "%s %d %f", stock, &shares, &price) == 3) {
                StockHolding *h = &p->holdings[p->stock_count++];
                strncpy(h->stock, stock, MAX_STOCK_NAME_LENGTH);
                h->shares = shares;
                h->avg_price = price;
            } else {
                // It’s a new username – backtrack line
                fseek(file, -strlen(line) - 1, SEEK_CUR);
                break;
            }
        }
    }
    fclose(file);
} // end load_portfolios



// tester function for printing loaded portfolios. Not called in the program. Only for testing. 
void test_load_portfolios(){
    //load_portfolios();
    for (int i = 0; i < portfolio_count; i++){
        printf("User: %s\n", portfolios[i].username);
        for (int j = 0; j < portfolios[i].stock_count; j++){
            StockHolding *h = &portfolios[i].holdings[j];
            printf("  Stock: %s, Shares: %d, Avg Price: %.2f\n", h->stock, h->shares, h->avg_price);
        }
    }
}