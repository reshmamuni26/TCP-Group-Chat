# TCP_Group_chat

Welcome to **TCP_Group_chat** — a simple multi-client chat server built in C++ using Windows sockets.  
I created this project using **Microsoft Visual Studio 2022** to help multiple users chat and share files over a network.

---

## What Is This?

This is a console-based server that listens for incoming TCP connections on port 12345.  
Once clients connect and register with a username, they can:

- Chat publicly with everyone
- Send private messages to specific users using `@username`  
- List who’s online with `/list`  
- Gracefully disconnect with `/quit`  
- Send files privately or to the whole group  

---

## How Does It Work?

1. The server starts up, sets up the Windows sockets system, and listens on port 12345.
2. When a client connects, the server reads the username and adds it to a list of active users.
3. Each client runs on its own thread, so multiple people can chat at the same time.
4. Messages are received and:
   - Broadcasted publicly to everyone else, or
   - Sent privately if the message starts with `@username`
5. If a message starts with `FILE:<size>:<filename>`, the server handles the file transfer and sends it to the right people.
6. When clients disconnect or send `/quit`, they are removed from the list, and everyone else gets notified.

---

## How to Build & Run

### What You Need

- Windows OS  
- Microsoft Visual Studio 2022  

### Steps

1. Clone or download this repo.  
2. Open the project in Visual Studio 2022.  
3. Build the solution (it links `ws2_32.lib` automatically).  
4. Run the server executable. You should see:  


Now your server is ready to accept client connections!

---

## Using the Server

Clients connect to the server via TCP on port 12345. The very first thing a client does is send their username. Then, they can send commands or chat messages:

- **Public message:** Just type and send! Everyone sees it.  
- **Private message:** Start your message with `@username` followed by your text.  
- **List users:** Type `/list` to get a list of everyone online.  
- **Quit:** Type `/quit` to leave the chat gracefully.  
- **Send a file:** Use `FILE:<filesize>:<filename>` followed by the file data.

---

## Screenshots

Here’s what the server and client look like in action!

### Server Console

<img width="1366" height="768" alt="server_output png" src="https://github.com/user-attachments/assets/740ce02c-6f50-4b5a-8530-5f3c328c4b4f" />


### Client Console

<img width="1366" height="768" alt="client_output png" src="https://github.com/user-attachments/assets/875e85ec-a75f-4d3c-85ae-e8fe18cdddf7" />


## A Few Things to Keep in Mind

- This is a simple learning project — no user authentication or encryption.  
- Messages are expected to fit into the buffer size (4096 bytes).  
- For file transfers, the client must send the correct header and exact file size.  
- Designed for small LAN use or practice — not production-ready.  

## How to Connect a Client

This project focuses on the server side, but here’s how a client typically works:

- The client connects to the server IP and port 12345.
- It sends the username as the first message.
- Then it can send commands (`/list`, `/quit`) or chat messages.
- To send private messages, start the message with `@username`.
- To send files, send a header like `FILE:<size>:<filename>` followed by the raw file bytes.

You can build your own client using similar Windows socket APIs or test with tools like **telnet** for basic text messaging.

---

## Code Overview

Here’s a quick look at the main components of the code:

- **`handleClient(SOCKET clientSock)`**: Handles communication with each connected client on its own thread.
- **`broadcastToAll()`**: Sends a message to all connected clients except the sender.
- **`recvAll()`**: Helper function that ensures receiving exactly the number of bytes expected (used mainly for file transfers).
- Uses a `std::map<string, SOCKET>` with a mutex (`usersMutex`) to safely track active users.

---

## Future Improvements

Some ideas for extending this project:

- Add user authentication (username and password).
- Implement encryption (e.g., TLS/SSL) for secure messaging.
- Support larger files by splitting into chunks or streaming.
- Create a GUI client for a better user experience.
- Add handling for client timeouts and automatic reconnections.
- Improve the communication protocol (e.g., use length-prefixed messages for better framing).

---

## Troubleshooting

- **WSAStartup failed:** Make sure your Windows system supports Winsock and your development environment is properly set up.
- **Port 12345 already in use:** Another application might be using the port. Change the port number or stop the conflicting app.
- **Clients can’t connect:** Check your firewall or antivirus settings to allow traffic on port 12345.
- **File transfer issues:** Verify that the file header and size are correctly sent before the raw file data.

---


