#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

class StockBot {
public:
    StockBot(const std::string& stockSymbol, double buyThreshold, double sellThreshold)
        : stockSymbol(stockSymbol), buyThreshold(buyThreshold), sellThreshold(sellThreshold), sockfd(-1) {
        std::srand(std::time(0)); 
    }

    ~StockBot() {
        if (sockfd != -1) {
            close(sockfd);
        }
    }

    bool connectToServer(const std::string& serverAddress, int port) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Socket creation failed.\n";
            return false;
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, serverAddress.c_str(), &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported.\n";
            return false;
        }

        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection failed.\n";
            return false;
        }

        std::cout << "Connected to " << serverAddress << " on port " << port << ".\n";
        return true;
    }

    void generateSignals() {
        double currentPrice = getCurrentStockPrice();
        int action = std::rand() % 2; // Randomly choose to buy or sell, and random quantity
        if (action == 0) {
            sendSignal("buy", currentPrice, std::rand() % 100 + 1); 
        } else {
            sendSignal("sell", currentPrice, std::rand() % 100 + 1);
        }
    }

private:
    std::string stockSymbol;
    double buyThreshold;
    double sellThreshold;
    int sockfd;

    double getCurrentStockPrice() {
        return (std::rand() % 200) + 50; // Random price between 50 and 250
    }

    void sendSignal(const std::string& action, double price, int quantity) {
        if (sockfd == -1) {
            std::cerr << "Not connected to server.\n";
            return;
        }

        std::string orderId = std::to_string(std::rand()); // Generate a random ID
        std::string message = orderId + " " + stockSymbol + " " + action + " " + std::to_string(price) + " " + std::to_string(quantity) + "\n";

        send(sockfd, message.c_str(), message.size(), 0);
        std::cout << "Sent: " << message;
    }
};

int main() {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10000; i++) {
        threads.emplace_back([]() {
            StockBot bot("AAPL", 150.00, 200.00);
            if (bot.connectToServer("127.0.0.1", 8080)) {
                bot.generateSignals();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
    std::cout << "All bots have finished.\n";

    system("python3 graph_html.py");
    return 0;
}