#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <memory>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iomanip>     
#include <filesystem>   

using namespace std;

const int PORT = 8080;
const string SERVER_DIR = "server_files/";



// Simple user database: username -> password
map<string, string> user_credentials = {
    {"user1", "pass1"},
    {"user2", "pass2"}
};

// Map to hold a mutex for each file, ensuring thread-safe file operations
map<string, shared_ptr<shared_mutex>> file_locks;
// Mutex to protect access to the file_locks map itself
mutex map_lock;

// Helper to safely get (or create) a mutex for a given filename
shared_ptr<shared_mutex> get_file_mutex(const string& filename) {
    lock_guard<mutex> guard(map_lock);
    if (file_locks.find(filename) == file_locks.end()) {
        file_locks[filename] = make_shared<shared_mutex>();
    }
    return file_locks[filename];
}

// --- File Operations ---

void sendFile(int clientSocket, const string &filename) {
    auto file_mutex = get_file_mutex(filename);
    shared_lock<shared_mutex> lock(*file_mutex); // READER lock

    string filepath = SERVER_DIR + filename;
    ifstream file(filepath, ios::binary);
    if (!file.is_open()) {
        size_t file_size = -1; // Indicate error
        send(clientSocket, &file_size, sizeof(file_size), 0);
        cout << "Client requested non-existent file: " << filename << endl;
        return;
    }

    file.seekg(0, ios::end);
    size_t total_size = file.tellg();
    file.seekg(0, ios::beg);
    send(clientSocket, &total_size, sizeof(total_size), 0);

    char buffer[4096];
    size_t sent = 0;
    auto start = chrono::steady_clock::now();

    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        streamsize bytes_read = file.gcount();
        if (bytes_read == 0) continue;
        send(clientSocket, buffer, bytes_read, 0);
        sent += bytes_read;

        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - start).count();
        double speed = (elapsed > 0) ? (sent / (1024.0 * 1024.0 * elapsed)) : 0;
        cout << "Uploading " << filename << " " << (100 * sent / total_size)
             << "% at " << fixed << setprecision(2) << speed << " MB/s\r" << flush;
    }
    cout << endl << filename << " sent successfully." << endl;
    file.close();
}

void receiveFile(int clientSocket, const string &filename) {
    auto file_mutex = get_file_mutex(filename);
    unique_lock<shared_mutex> lock(*file_mutex); // WRITER lock

    string filepath = SERVER_DIR + filename;
    ofstream file(filepath, ios::binary);

    size_t total_size;
    recv(clientSocket, &total_size, sizeof(total_size), 0);
    if (total_size == -1) {
        cout << "Client failed to send file: " << filename << endl;
        return;
    }

    char buffer[4096];
    size_t received = 0;
    auto start = chrono::steady_clock::now();

    while (received < total_size) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        file.write(buffer, bytes);
        received += bytes;

        auto now = chrono::steady_clock::now();
        double elapsed = chrono::duration<double>(now - start).count();
        double speed = (elapsed > 0) ? (received / (1024.0 * 1024.0 * elapsed)) : 0;
        cout << "Downloading " << filename << " " << (100 * received / total_size)
             << "% at " << fixed << setprecision(2) << speed << " MB/s\r" << flush;
    }
    cout << endl << filename << " received successfully." << endl;
    file.close();
}

void listFiles(int clientSocket) {
    stringstream ss;
    ss << "--- Server Files ---\n";
    for (const auto& entry : filesystem::directory_iterator(SERVER_DIR)) {
        if (entry.is_regular_file()) {
            ss << entry.path().filename().string() << "\n";
        }
    }
    string file_list = ss.str();
    send(clientSocket, file_list.c_str(), file_list.length(), 0);
}

void deleteFile(int clientSocket, const string& filename, bool isAuthenticated) {
    if (!isAuthenticated) {
        string msg = "ERROR: Permission denied. Please log in to delete files.";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    auto file_mutex = get_file_mutex(filename);
    unique_lock<shared_mutex> lock(*file_mutex); // WRITER lock

    string filepath = SERVER_DIR + filename;
    string msg;
    if (remove(filepath.c_str()) == 0) {
        msg = "SUCCESS: File '" + filename + "' deleted.";
    } else {
        msg = "ERROR: File '" + filename + "' not found or could not be deleted.";
    }
    send(clientSocket, msg.c_str(), msg.length(), 0);
}

// --- Client Handler ---

void handleClient(int clientSocket) {
    cout << "Client connected." << endl;
    bool isAuthenticated = false;
    string username;

    char buffer[1024] = {0};
    while (true) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        
        stringstream ss(buffer);
        string command;
        ss >> command;

        if (command == "LOGIN") {
            string password;
            ss >> username >> password;
            if (user_credentials.count(username) && user_credentials[username] == password) {
                isAuthenticated = true;
                string msg = "SUCCESS: Login successful. Welcome " + username;
                send(clientSocket, msg.c_str(), msg.length(), 0);
            } else {
                isAuthenticated = false;
                string msg = "ERROR: Invalid username or password.";
                send(clientSocket, msg.c_str(), msg.length(), 0);
            }
        } else if (command == "LIST") {
            listFiles(clientSocket);
        } else if (command == "UPLOAD") {
            string filename;
            ss >> filename;
            receiveFile(clientSocket, filename);
        } else if (command == "DOWNLOAD") {
            string filename;
            ss >> filename;
            sendFile(clientSocket, filename);
        } else if (command == "DELETE") {
            string filename;
            ss >> filename;
            deleteFile(clientSocket, filename, isAuthenticated);
        } else if (command == "EXIT") {
            break;
        }
    }
    cout << "Client disconnected." << endl;
    close(clientSocket);
}

// --- Main Server Logic ---

int main() {
    mkdir(SERVER_DIR.c_str(), 0777);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);
    cout << "Server listening on port " << PORT << endl;

    while (true) {
        int clientSocket = accept(server_fd, nullptr, nullptr);
        thread(handleClient, clientSocket).detach();
    }
    close(server_fd);
    return 0;
}