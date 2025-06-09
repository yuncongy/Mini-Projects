Project Name: Mini Stock Trading System

Project Description:

This project implements a client-server stock trading system using both TCP and UDP protocols. The system is designed to handle user authentication, portfolio management, stock quoting, buying, and selling functionalities.

TCP connection between client and server M (Main Server). Port # dynamically allocated. 
UDP connection between server M and server A, P, Q (Authentication, Portfolio, Quote). Port # fixed. 
* client can only communicate with server M. M forwards request to designated servers. *


Key Features:
User Authentication: Supports user login with username and password.
Stock Quoting: Allows clients to request quotes for all available stocks or a specific stock.
Portfolio Management: Tracks user stock holdings and calculates profit/loss.
Buy/Sell Functionality: Enables users to buy and sell stocks, with confirmations and success/failure messages.
Robust Communication: Utilizes TCP for reliable client-to-main server communication and UDP for efficient main server-to-backend server interactions.
The project also includes a Makefile for compilation and cleanup , and some variable sizes are limited (e.g., max 10 clients, 100 stocks). Some code related to TCP and UDP connections was reused from Beej's Guide.


Content:
client.c - handle and parse client input, send correct message via TCP to server M for further processing. Display message based on reply from serverM.
serverM.c - Main server, receive client input via TCP and send/receive corresponding message to/from backend servers via UDP, then send the result back to client
serverA.c - handles user authentication. Receive username and encrypted password from serverM and verify, send the result back to serverM via UDP
serverQ.c - handle quote. Receive quote all or quote specific stock from serverM and sends the price of stocks back to serverM via UDP
serverP.c - handle the portfolio of each user. Update the user's portfolio during buy sell action. handle the position request from serverM.
Makefile - compile all the necessary .c files using make all, and remove executables using make clean



Message exchanged
- TCP Connection: (client, Server M)
LOGIN,<username>,<password>
LOGIN_SUCCESS
LOGIN_FAILED
QUOTE_ALL
QUOTE <stock>
<stock>,<price>,<stock>,<price>,.....
POSITION
Stock list + profit info
SELL <stock> <shares>
CONFIRM_SELL,<price>
Y or N
SELL_SUCCESS,<user>,<stock>,<shares>
SELL_FAIL,<reason>
BUY <stock> <shares>
CONFIRM_BUY,<price>
Y or N
BUY_SUCCESS,<user>,<stock>,<shares>
BUY_FAIL,<reason>
stock not found

- UDP Connection: (Server M, P, Q, A)
LOGIN,<username>,<encrypted_password>
1 or 0
QUOTE_ALL
QUOTE,<stock>
<stock1>,<price1>,<stock2>,<price2>,...
POSITION,<username>,<stock1>,<price1>,<stock2>,<price2>,... 
Stock list + profit info
SELL,<username>,<stock>,<shares> 
CONFIRM_SELL,<username>,<stock>,<shares>
SELL_FAIL,<username>,<stock>,<shares>
CONFIRM_SELL,<username>,<stock>,<shares>
SELL_SUCCESS,<username>,<stock>,<shares>
SELL_FAIL,<username>,<stock>,<shares>
DENIED_SELL
BUY,<username>,<stock>,<shares>
BUY_SUCCESS,<username>,<stock>,<shares>
BUY_FAIL,<username>,<stock>,<shares>


Several variable are limited by size:
- max number of client 10
- max number of stocks 100
- max username password length 98
- max stock name length 5
input format are limited to the ones mentioned in the message prompt.
If user enter wrong/incorrect format, client terminal will print invalid command.

All input files must be located in the same directory. 
./quote.txt ...etc


