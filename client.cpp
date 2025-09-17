#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;

const int PORT = 8080;
const string CLIENT_DIR = "client_files/";

void receive_server_message(int sock) {
    char buffer[4096] = {0};
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        cout << buffer << endl;
    }
}

void sendFile(int sock, const string &filename) {
    string filepath = CLIENT_DIR + filename;
    ifstream file(filepath, ios::binary);
    if (!file.is_open()) {
        cout << "ERROR: File not found in client_files/ directory." << endl;
        size_t file_size = -1; // Indicate error
        send(sock, &file_size, sizeof(file_size), 0);
        return;
    }

    file.seekg(0, ios::end);
    size_t total_size = file.tellg();
    file.seekg(0, ios::beg);
    send(sock, &total_size, sizeof(total_size), 0);

    char buffer[4096];
    size_t sent = 0;
    auto start = chrono::steady_clock::now();

    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        streamsize bytes_read = file.gcount();
        if (bytes_read == 0) continue;
        send(sock, buffer, bytes_read, 0);
        sent += bytes_read;
        
        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - start).count();
        double speed = (elapsed > 0) ? (sent / (1024.0 * 1024.0 * elapsed)) : 0;
        cout << "Uploading " << filename << " " << (100 * sent / total_size)
             << "% at " << fixed << setprecision(2) << speed << " MB/s\r" << flush;
    }
    cout << endl << "Upload complete." << endl;
    file.close();
}

void receiveFile(int sock, const string &filename) {
    string filepath = CLIENT_DIR + filename;
    ofstream file(filepath, ios::binary);

    size_t total_size;
    recv(sock, &total_size, sizeof(total_size), 0);
    if (total_size == -1) {
        cout << "ERROR: File not found on server or server error." << endl;
        return;
    }

    char buffer[4096];
    size_t received = 0;
    auto start = chrono::steady_clock::now();

    while (received < total_size) {
        int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        file.write(buffer, bytes);
        received += bytes;
        
        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - start).count();
        double speed = (elapsed > 0) ? (received / (1024.0 * 1024.0 * elapsed)) : 0;
        cout << "Downloading " << filename << " " << (100 * received / total_size)
             << "% at " << fixed << setprecision(2) << speed << " MB/s\r" << flush;
    }
    cout << endl << "Download complete." << endl;
    file.close();
}

void print_help() {
    cout << "\n--- Available Commands ---\n"
         << "LOGIN <username> <password>  - Log in to the server (e.g., LOGIN user1 pass1)\n"
         << "LIST                         - List files on the server\n"
         << "UPLOAD <filename>            - Upload a file from client_files/\n"
         << "DOWNLOAD <filename>          - Download a file to client_files/\n"
         << "DELETE <filename>            - Delete a file on the server (requires login)\n"
         << "EXIT                         - Disconnect from the server\n"
         << "HELP                         - Show this help message\n"
         << "--------------------------" << endl;
}

int main() {
    mkdir(CLIENT_DIR.c_str(), 0777);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Connection Failed. Is the server running?" << endl;
        return -1;
    }

    cout << "Connected to the server. Type 'HELP' for commands." << endl;

    string line;
    while (true) {
        cout << "> ";
        getline(cin, line);
        if (line.empty()) continue;

        stringstream ss(line);
        string command;
        ss >> command;

        if (command == "HELP") {
            print_help();
            continue;
        }

        send(sock, line.c_str(), line.length(), 0);

        if (command == "EXIT") {
            break;
        } else if (command == "UPLOAD") {
            string filename;
            ss >> filename;
            if (filename.empty()) {
                cout << "Please specify a filename." << endl;
                continue;
            }
            sendFile(sock, filename);
        } else if (command == "DOWNLOAD") {
            string filename;
            ss >> filename;
            if (filename.empty()) {
                cout << "Please specify a filename." << endl;
                continue;
            }
            receiveFile(sock, filename);
        } else if (command == "LOGIN" || command == "LIST" || command == "DELETE") {
            receive_server_message(sock);
        } else {
             cout << "Unknown command. Type 'HELP' for a list of commands." << endl;
        }
    }
    
    cout << "Disconnecting from server." << endl;
    close(sock);
    return 0;
}