# C++ Concurrent File Transfer Protocol

A multi-threaded, command-line file server built in C++ using TCP Sockets. This project demonstrates advanced concepts in networking, concurrency, and synchronization, allowing multiple clients to simultaneously upload, download, list, and delete files in a thread-safe manner.

---
##  Key Features

* **Multi-threaded Server**: Handles multiple clients concurrently, with each client managed in a separate thread.
* **User Authentication**: A server-side login system is required for privileged actions like deleting files.
* **Full File Operations**: Clients can `UPLOAD`, `DOWNLOAD`, `LIST`, and `DELETE` files from the server.
* **Advanced Concurrency Control**: Implements a **Reader-Writer Lock** (`std::shared_mutex`) to prevent race conditions. This ensures a file cannot be deleted while another client is downloading it, while still allowing multiple clients to download the same file simultaneously.
* **Interactive CLI Client**: A simple and intuitive command-line interface for interacting with the server.

---

### Compilation


1.  **Compile the server:**
    ```sh
    g++ server.cpp -o server  -lpthread
    ```

2.  **Compile the client:**
    ```sh
    g++ client.cpp -o client  -lpthread
    ```

You can run the client and server on the same machine or on two different machines on the same local network.

### On a Single Machine (Localhost)

1.  **Start the Server**:
    ```sh
    ./server
    ```
2.  **Start the Client**:
    Open a new terminal and run:
    ```sh
    ./client
    ```

### On Two Different Machines (on the same network)

1.  **Find the Server's Local IP**: On the server machine, run `ip addr show` (Linux/macOS) or `ipconfig` (Windows) to find its local IP address (e.g., `192.168.1.10`).
2.  **Modify the Client Code**: Open `client.cpp` and change the IP address in this line:
    ```cpp
    // Change "127.0.0.1" to your server's IP address
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    ```
3.  **Run**: Start the server on the server machine and the newly compiled client on the client machine.
   

### Client Commands

Once connected, you can use the following commands. Type `HELP` to see this list in the client.

| Command | Description |
| :--- | :--- |
| `LOGIN <user> <pass>` | Log in to the server. Required for deleting files. |
| `LIST` | List all available files on the server. |
| `UPLOAD <filename>` | Upload a file from your `client_files/` directory. |
| `DOWNLOAD <filename>`| Download a file to your `client_files/` directory. |
| `DELETE <filename>` | Delete a file from the server. **Login is required.** |
| `HELP` | Displays the command list. |
| `EXIT` | Disconnects from the server and closes the client. |

---

## Concurrency Model
The server uses a **one-thread-per-client** model. The main thread listens for incoming connections and upon accepting one, it spawns a new `thread` dedicated to handling all communication with that specific client. This allows the server to manage multiple clients simultaneously without blocking.

### Synchronization
To handle race conditions between reading (downloading) and writing (deleting), the server employs a `shared_mutex` for each file.

* **`DOWNLOAD` (Read Operation)**: Acquires a `shared_lock`. Multiple clients can acquire this lock on the same file, allowing for concurrent downloads.
* **`DELETE` or `UPLOAD` (Write Operation)**: Acquires a `unique_lock`. This lock can only be obtained if **no other locks** are held. If a client is downloading a file, any request to delete it will be blocked until the download is complete, ensuring data integrity.
