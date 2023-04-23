#include "server.h"
#include "client.h"
#include "crypto.h"
#include <random>
#include <regex>

std::vector<std::string> pending_trxs;

Server::Server() {};

std::shared_ptr<Client> Server::add_client(std::string id)
{
    // id has already used
    for(const auto &item : clients) {
        if (item.first->get_id() == id) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> distr(1000, 9999);
            int number = distr(gen);
            id += std::to_string(number);
            break;
        }
    } 
    auto client = std::make_shared<Client>(id, *this);
    clients[client] = 5;
    return client;
}

std::shared_ptr<Client> Server::get_client(std::string id) const
{
    for(const auto &item : clients) {
        if (item.first->get_id() == id) {
            return item.first;
        }
    }    
    return nullptr;
}

double Server::get_wallet(std::string id) const
{
    for(const auto &item : clients) {
        if (item.first->get_id() == id) {
            return item.second;
        }
    }
    return -1;
}

bool Server::parse_trx(std::string trx, std::string &sender, std::string &receiver, double &value)
{
    std::regex regex("-");
    std::vector<std::string> out(std::sregex_token_iterator(trx.begin(), trx.end(), regex, -1),
                                std::sregex_token_iterator());
    if (out.size() != 3) {
        throw std::runtime_error("not standard");
    }
    sender = out[0];
    receiver = out[1];
    value = std::stod(out[2]);
    return true;
}

bool Server::add_pending_trx(std::string trx, std::string signature) const
{
    std::string sender, receiver;
    double value;
    parse_trx(trx, sender, receiver, value);
    const auto client = get_client(sender);
    const auto client_recv = get_client(receiver);
    if (!client || !client_recv) {
        return false;
    }
    bool authentic = crypto::verifySignature(client->get_publickey(), trx, signature);
    if (authentic) {
        double wallet = client->get_wallet();
        if (wallet < value) {
            return false;
        }
        pending_trxs.push_back(trx);
        return true;
    }
    return false;
}

size_t Server::mine()
{
    std::string mempool;
    for(const auto &trx : pending_trxs) {
        mempool += trx;
    }
    
    while(true) {
        for(const auto [client, wallet] : clients) {
            size_t nonce = client->generate_nonce();
            std::string hash{crypto::sha256(mempool + std::to_string(nonce))};
            int cnt = 0;
            for(int i = 0; i < 10; i++) {
                if (hash[i] == '0') {
                    cnt++;
                }
            }
            if (cnt >= 3) {
                clients[client] += 6.25;
                std::cout << "Success mine id: " << client->get_id() << std::endl;
                
                // settle accounts
                for(const auto &trx : pending_trxs) {
                    std::string sender, receiver;
                    double value;
                    parse_trx(trx, sender, receiver, value);
                    const auto &client_send = get_client(sender);
                    const auto &client_recv = get_client(receiver);
                    clients[client_send] -= value;
                    clients[client_recv] += value;
                }

                return nonce;
            }
        }
    }
    return -1;
}


void show_wallets(const Server &server)
{
    std::cout << std::string(20, '*') << std::endl;
    for (const auto &client : server.clients)
        std::cout << client.first->get_id() << " : " << client.second << std::endl;
    std::cout << std::string(20, '*') << std::endl;
}

