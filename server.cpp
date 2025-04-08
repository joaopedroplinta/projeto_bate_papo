#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "chat.pb.h"

std::unordered_map<int, std::string> clients;  // Map de socket -> nome do cliente
std::unordered_map<int, std::string> clients_names;  // Map de socket -> nome do cliente
std::mutex clients_mutex;  // Mutex para proteger o acesso à lista de clientes

// Função para enviar mensagens para todos os clientes conectados
void send_to_all(const std::string& message, int sender_socket) {
    BCC_Dist_toClient send_message;
    send_message.set_command(BCC_Dist_toClient::CMD_MSG);
    
    BCC_formatMessage* msg = send_message.mutable_message();
    msg->set_message(message);
    msg->set_source(clients[sender_socket]);

    std::string serialized_message;
    send_message.SerializeToString(&serialized_message);

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.first != sender_socket) {  // Não enviar para o próprio cliente
            send(client.first, serialized_message.data(), serialized_message.size(), 0);
        }
    }
}

// Função para enviar mensagem para um único cliente
void send_to_one(const std::string& message, int sender_socket, const std::string& receiver_name) {
    BCC_Dist_toClient send_message;
    send_message.set_command(BCC_Dist_toClient::CMD_MSG);

    BCC_formatMessage* msg = send_message.mutable_message();
    msg->set_message(message);
    msg->set_source(clients[sender_socket]);
    msg->set_receiver(receiver_name);  // Definindo o destinatário

    std::string serialized_message;
    send_message.SerializeToString(&serialized_message);

    std::lock_guard<std::mutex> lock(clients_mutex);
    // Enviar a mensagem para o cliente correto
    for (const auto& client : clients) {
        if (client.second == receiver_name) {  // Encontrou o cliente pelo nome
            send(client.first, serialized_message.data(), serialized_message.size(), 0);
            break;
        }
    }
}

// Função para enviar lista de usuários conectados
void handle_list_command(int client_socket) {
    BCC_Dist_toClient list_message;
    list_message.set_command(BCC_Dist_toClient::CMD_LIST);

    // Adiciona os nomes de todos os clientes conectados
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        BCC_users* user = list_message.add_users();  // Alteração: usaremos 'BCC_users'
        user->set_name(client.second);  // Adiciona o nome do cliente à lista
    }

    // Envia a lista de usuários para o cliente que solicitou
    std::string serialized_list;
    list_message.SerializeToString(&serialized_list);
    send(client_socket, serialized_list.data(), serialized_list.size(), 0);
}

// Função para lidar com a desconexão de um cliente
void handle_disconnect(int client_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.erase(client_socket);  // Remove o cliente da lista de conectados
}

// Função para receber mensagens dos clientes
void handle_client(int client_socket) {
    char buffer[4096];
    while (true) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cerr << "Client " << clients[client_socket] << " disconnected.\n";
            break;
        }

        BCC_Dist_toServer message;
        if (message.ParseFromArray(buffer, bytes_received)) {
            if (message.command() == BCC_Dist_toServer::CMD_SENDALL) {
                send_to_all(message.message(), client_socket);  // Envia para todos
            } else if (message.command() == BCC_Dist_toServer::CMD_LIST) {
                handle_list_command(client_socket);  // Lista usuários
            } else if (message.command() == BCC_Dist_toServer::CMD_SENDONE) {
                // Envia a mensagem para um único usuário
                const std::string& receiver_name = message.receivers(0).name();
                send_to_one(message.message(), client_socket, receiver_name);
            } else if (message.command() == BCC_Dist_toServer::CMD_DC) {
                handle_disconnect(client_socket);  // Desconectar
                break;
            }
        } else {
            std::cerr << "Failed to parse client message.\n";
        }
    }

    close(client_socket);
}

// Função para aceitar conexões dos clientes
void accept_connections(int server_socket) {
    while (true) {
        sockaddr_in client_address;
        socklen_t client_addr_len = sizeof(client_address);
        int client_socket = accept(server_socket, (sockaddr*)&client_address, &client_addr_len);
        if (client_socket == -1) {
            std::cerr << "Failed to accept client connection.\n";
            continue;
        }

        // Recebe o nome do cliente
        char buffer[4096];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            BCC_Dist_toServer init_message;
            if (init_message.ParseFromArray(buffer, bytes_received)) {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients[client_socket] = init_message.myname();  // Adiciona o nome do cliente
                std::cout << "New connection: " << clients[client_socket] << "\n";
            }
        }

        // Cria uma thread para lidar com o cliente
        std::thread(handle_client, client_socket).detach();
    }
}

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Error creating socket.\n";
        return -1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Error binding server socket.\n";
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) == -1) {
        std::cerr << "Error listening on server socket.\n";
        close(server_socket);
        return -1;
    }

    std::cout << "Server is listening on port 8080...\n";
    accept_connections(server_socket);  // Começa a aceitar conexões

    close(server_socket);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
