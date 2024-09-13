#include <iostream>
#include <string>
#include <thread>
#include <map>
#include <vector>
#include <mutex>
#include <sstream>
#include <condition_variable>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <queue>
#include <fstream>
using namespace std;

struct Order {
    string orderId;
    string symbol;
    string side; 
    double price;
    int quantity;
};

class OrderBook {
private:
    map<string, vector<Order>> buyOrders;
    map<string, vector<Order>> sellOrders;
    mutex mtx;
    ofstream logFile;

public:
    OrderBook() {
        logFile.open("order_log.txt", ios::out | ios::app);
    }

    ~OrderBook() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    void addOrder(Order order) {
        lock_guard<mutex> lock(mtx);
        if (order.side == "buy") {
            buyOrders[order.symbol].push_back(order);
        } else if (order.side == "sell") {
            sellOrders[order.symbol].push_back(order);
        }
        logFile << "Order added: " << order.orderId << ", Symbol: " << order.symbol 
                << ", Side: " << order.side << ", Price: " << order.price 
                << ", Quantity: " << order.quantity << endl;
    }

    void matchOrders(const string& symbol) {
    lock_guard<mutex> lock(mtx);
    if (buyOrders[symbol].empty() || sellOrders[symbol].empty()) return;

    while (!buyOrders[symbol].empty() && !sellOrders[symbol].empty()) {
        Order& buyOrder = buyOrders[symbol].back();
        Order& sellOrder = sellOrders[symbol].back();

        if (buyOrder.price >= sellOrder.price) {
            int matchQty = min(buyOrder.quantity, sellOrder.quantity);
            buyOrder.quantity -= matchQty;
            sellOrder.quantity -= matchQty;

            logFile << "Matched Order! Buy Order ID: " << buyOrder.orderId
                    << " Sell Order ID: " << sellOrder.orderId 
                    << " Quantity: " << matchQty << endl;

            if (buyOrder.quantity == 0) buyOrders[symbol].pop_back();
            if (sellOrder.quantity == 0) sellOrders[symbol].pop_back();
        } else {
            break;
        }
    }
}

    pair<double, double> getCurrentPrices(const string& symbol) {
        lock_guard<mutex> lock(mtx);
        double buyPrice = buyOrders[symbol].empty() ? 0.0 : buyOrders[symbol].back().price;
        double sellPrice = sellOrders[symbol].empty() ? 0.0 : sellOrders[symbol].back().price;
        return {buyPrice, sellPrice};
    }
};

class OrderServer {
private:
    OrderBook& orderBook;
    int server_fd;
    mutex mtx;
    condition_variable cv;
    queue<Order> orderQueue;
    ofstream logFile;

public:
    OrderServer(OrderBook& book) : orderBook(book), server_fd(-1) {
        logFile.open("order_log.txt", ios::out | ios::app);
    }

    ~OrderServer() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    void startServer(int port) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == 0) {
            cerr << "Socket creation failed." << endl;
            return;
        }

        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);

        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            cerr << "Bind failed." << endl;
            return;
        }

        if (listen(server_fd, 3) < 0) {
            cerr << "Listen failed." << endl;
            return;
        }

        cout << "Server started on port " << port << endl;

        while (true) {
            int new_socket;
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                cerr << "Accept failed." << endl;
                return;
            }

            thread(&OrderServer::handleConnection, this, new_socket).detach();
        }
    }

    void serveOrderBook(int socket) {
        auto prices = orderBook.getCurrentPrices("AAPL"); // Example for AAPL
        stringstream response;
        response << "{ \"buyPrice\": " << prices.first << ", \"sellPrice\": " << prices.second << " }";
    
        string responseStr = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + response.str();
        send(socket, responseStr.c_str(), responseStr.size(), 0);
    }

    void handleConnection(int socket) {
        char buffer[1024] = {0};
        read(socket, buffer, 1024);

        string request(buffer);
        if (request.find("GET /orderbook") != string::npos) {
            serveOrderBook(socket);
        } else {
            string orderData(buffer);
            Order order = parseOrder(orderData);

            {
                lock_guard<mutex> lock(mtx);
                orderQueue.push(order);
            }
            cv.notify_one();
        }

        close(socket);
    }

    void processOrders() {
        while (true) {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [&]() { return !orderQueue.empty(); });

            Order order = orderQueue.front();
            orderQueue.pop();
            lock.unlock();

            logFile << "Processing Order: " << order.orderId << ", Symbol: " << order.symbol 
                    << ", Side: " << order.side << ", Price: " << order.price 
                    << ", Quantity: " << order.quantity << endl;

            orderBook.addOrder(order);
            orderBook.matchOrders(order.symbol);
        }
    }

    Order parseOrder(const string& data) {
        Order order;
        istringstream ss(data);
        ss >> order.orderId >> order.symbol >> order.side >> order.price >> order.quantity;
        return order;
    }
};

int main() {
    //TODO: clean log
    ofstream logFile;
    logFile.open("order_log.txt", ios::out | ios::trunc);
    logFile.close();

    OrderBook orderBook;
    OrderServer server(orderBook);

    thread serverThread([&]() {
        server.startServer(8080);
    });

    thread orderProcessingThread([&]() {
        server.processOrders();
    });

    serverThread.join();
    orderProcessingThread.join();
    return 0;
}