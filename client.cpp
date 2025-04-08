#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "chat.pb.h"

void receive_messages(int server_socket) {
    while (true) {
        char buffer[4096];
        ssize_t bytes_received = recv(server_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cerr << "Disconnected from server.\n";
            break;
        }

        BCC_Dist_toClient message;
        if (message.ParseFromArray(buffer, bytes_received)) {
            if (message.command() == BCC_Dist_toClient::CMD_MSG) {
                const auto& msg = message.message();
                std::cout << "[" << msg.source() << "]: " << msg.message() << "\n";
            } else if (message.command() == BCC_Dist_toClient::CMD_LIST) {
                std::cout << "Users online:\n";
                for (const auto& user : message.users()) {
                    std::cout << "- " << user.name() << "\n";
                }
            }
        } else {
            std::cerr << "Failed to parse server message.\n";
        }
    }
}

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // Configuração do cliente
    std::string client_name;
    std::cout << "Enter your name: ";
    std::getline(std::cin, client_name);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create socket.\n";
        return -1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    if (connect(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Failed to connect to server.\n";
        close(server_socket);
        return -1;
    }

    std::cout << "Connected to the server.\n";

    // Thread para receber mensagens do servidor
    std::thread(receive_messages, server_socket).detach();

    // Enviar nome do cliente ao servidor
    BCC_Dist_toServer init_message;
    init_message.set_command(BCC_Dist_toServer::CMD_ID);
    init_message.set_myname(client_name);

    std::string serialized_message;
    init_message.SerializeToString(&serialized_message);
    send(server_socket, serialized_message.data(), serialized_message.size(), 0);

    // Loop principal para interagir com o servidor
    while (true) {
        std::cout << "\nCommands:\n"
                  << "/list - List online users\n"
                  << "/sendall - Send message to all users\n"
                  << "/sendone - Send message to one user\n"
                  << "/quit - Disconnect from the server\n"
                  << "Enter command: ";

        std::string command;
        std::getline(std::cin, command);

        if (command == "/quit") {
            BCC_Dist_toServer disconnect_message;
            disconnect_message.set_command(BCC_Dist_toServer::CMD_DC);

            std::string serialized_disconnect;
            disconnect_message.SerializeToString(&serialized_disconnect);
            send(server_socket, serialized_disconnect.data(), serialized_disconnect.size(), 0);
            break;
        } else if (command == "/list") {
            BCC_Dist_toServer list_message;
            list_message.set_command(BCC_Dist_toServer::CMD_LIST);

            std::string serialized_list;
            list_message.SerializeToString(&serialized_list);
            send(server_socket, serialized_list.data(), serialized_list.size(), 0);
        } else if (command == "/sendall") {
            std::cout << "Enter your message: ";
            std::string message;
            std::getline(std::cin, message);

            BCC_Dist_toServer send_message;
            send_message.set_command(BCC_Dist_toServer::CMD_SENDALL);
            send_message.set_message(message);

            std::string serialized_send;
            send_message.SerializeToString(&serialized_send);
            send(server_socket, serialized_send.data(), serialized_send.size(), 0);
        } else if (command == "/sendone") {
            std::cout << "Enter recipient's name: ";
            std::string recipient;
            std::getline(std::cin, recipient);

            std::cout << "Enter your message: ";
            std::string message;
            std::getline(std::cin, message);

            BCC_Dist_toServer send_message;
            send_message.set_command(BCC_Dist_toServer::CMD_SENDONE);
            send_message.set_message(message);

            auto* receiver = send_message.add_receivers();
            receiver->set_name(recipient);

            std::string serialized_send;
            send_message.SerializeToString(&serialized_send);
            send(server_socket, serialized_send.data(), serialized_send.size(), 0);
        } else {
            std::cout << "Unknown command. Please try again.\n";
        }
    }

    close(server_socket);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
