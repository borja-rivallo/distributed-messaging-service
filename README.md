# Distributed Messaging Service

A client-server messaging architecture built with C and Python. It manages concurrent user connections via TCP sockets and ensures operational auditing through a secondary Sun RPC logging server.

## Technical Features
* **Concurrency:** Multithreaded C server using POSIX threads (`pthreads`) to handle multiple client connections simultaneously without blocking.
* **Network Communication:** Low-level TCP sockets implementation for robust message routing and file transmission in 4kB blocks.
* **Distributed Auditing:** Sun RPC integration to log every system operation (registration, connection, messages) to an independent registry server.
* **Web Service Integration:** Python clients consume a local SOAP web service (built with Zeep and Spyne) to normalize message text before transmission.

## Compilation & Execution
1. Build the project:

   ```bash
   make
   ```
2. Start the servers:

   ```bash
   # Terminal 1
   ./rpc_server
   ```
   ```bash
   # Terminal 2
   export LOG_RPC_IP=<RPC_SERVER_IP>
   ./server -p <SERVER_PORT>
   ```
2. Start the message normalization web service:

   ```bash
   python3 conversor_mensajes.py
   ```
3. Setup Python environment & web service:

   ```bash
   make setup
   python3 conversor_mensajes.py &
   ```
4. Run the client(s):

   ```bash
   python3 client.py -s <MAIN_SERVER_IP> -p <PORT>
   ```
