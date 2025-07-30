#define WIN32_LEAN_AND_MEAN
#include <windows.h>     // Windows socket APIs
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>      // std::cout, std::cerr
#include <thread>        // std::thread
#include <map>           // std::map
#include <mutex>         // std::mutex
#include <vector>        // std::vector
#include <string>        // std::string

#pragma comment(lib, "ws2_32.lib")  // Link Winsock library

using namespace std;

const int PORT = 12345;
const int BUF_SIZE = 4096;

// Mapping from username ? client socket
map<string, SOCKET> users;
mutex usersMutex;  // Protects access to users map

// Helper to receive exactly 'len' bytes
bool recvAll(SOCKET sock, char* buf, int len) {
    int total = 0;
    while (total < len) {
        int r = recv(sock, buf + total, len - total, 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

// Broadcast message to all except optionally one sender
void broadcastToAll(const string& msg, SOCKET sender) {
    lock_guard<mutex> lock(usersMutex);
    for (map<string, SOCKET>::iterator it = users.begin(); it != users.end(); ++it) {
        SOCKET s = it->second;
        if (s != sender) {
            send(s, msg.c_str(), (int)msg.size(), 0);
        }
    }
}

// Handles communication per client
void handleClient(SOCKET clientSock) {
    char buf[BUF_SIZE];
    int received = recv(clientSock, buf, BUF_SIZE - 1, 0);
    if (received <= 0) {
        closesocket(clientSock);
        return;
    }
    buf[received] = '\0';
    string username = buf;

    {
        lock_guard<mutex> lock(usersMutex);
        users[username] = clientSock;
    }

    broadcastToAll("[SERVER] " + username + " joined\n", clientSock);
    cout << username << " connected\n";

    while (true) {
        received = recv(clientSock, buf, BUF_SIZE - 1, 0);
        if (received <= 0) break;

        buf[received] = '\0';
        string msg = buf;

        if (msg == "/quit") {
            break;
        }

        if (msg == "/list") {
            // Build list of active users
            string listMsg = "[SERVER] Active users:\n";
            lock_guard<mutex> lock(usersMutex);
            for (map<string, SOCKET>::iterator it = users.begin(); it != users.end(); ++it) {
                listMsg += "- " + it->first + "\n";
            }
            send(clientSock, listMsg.c_str(), (int)listMsg.size(), 0);
            continue;
        }

        // Private message or file
        if (!msg.empty() && msg[0] == '@') {
            size_t spacePos = msg.find(' ');
            if (spacePos != string::npos) {
                string target = msg.substr(1, spacePos - 1);
                string rest = msg.substr(spacePos + 1);
                SOCKET targetSock = INVALID_SOCKET;

                lock_guard<mutex> lock(usersMutex);
                if (users.count(target)) {
                    targetSock = users[target];
                }

                if (targetSock == INVALID_SOCKET) {
                    string err = "[SERVER] User not found: " + target + "\n";
                    send(clientSock, err.c_str(), (int)err.size(), 0);
                }
                else {
                    if (rest.rfind("FILE:", 0) == 0) {
                        // Private file header
                        send(targetSock, rest.c_str(), (int)rest.size(), 0);
                        size_t p1 = rest.find(':');
                        size_t p2 = rest.find(':', p1 + 1);
                        if (p1 == string::npos || p2 == string::npos) continue;

                        int fsz = atoi(rest.substr(p1 + 1, p2 - p1 - 1).c_str());
                        vector<char> fileData(fsz);
                        if (!recvAll(clientSock, fileData.data(), fsz)) break;

                        send(targetSock, fileData.data(), fsz, 0);
                    }
                    else {
                        string pm = "@" + username + " " + rest;
                        send(targetSock, pm.c_str(), (int)pm.size(), 0);
                    }
                }
            }
            continue;
        }

        // Group file transfer
        if (msg.rfind("FILE:", 0) == 0) {
            broadcastToAll(msg, clientSock);  // header to others

            size_t p1 = msg.find(':');
            size_t p2 = msg.find(':', p1 + 1);
            if (p1 == string::npos || p2 == string::npos) continue;

            int fsz = atoi(msg.substr(p1 + 1, p2 - p1 - 1).c_str());
            vector<char> fileData(fsz);
            if (!recvAll(clientSock, fileData.data(), fsz)) break;

            lock_guard<mutex> lock(usersMutex);
            for (map<string, SOCKET>::iterator it = users.begin(); it != users.end(); ++it) {
                if (it->second != clientSock) {
                    send(it->second, fileData.data(), fsz, 0);
                }
            }
            continue;
        }

        // Public text message
        broadcastToAll(username + ": " + msg + "\n", clientSock);
    }

    // Handle disconnection
    {
        lock_guard<mutex> lock(usersMutex);
        users.erase(username);
    }
    broadcastToAll("[SERVER] " + username + " left\n", clientSock);
    closesocket(clientSock);
    cout << username << " disconnected\n";
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        cerr << "socket() failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in servAddr = {};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(PORT);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (sockaddr*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
        cerr << "bind() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "listen() failed\n";
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    cout << "[SERVER] Listening on port " << PORT << "...\n";

    while (true) {
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET) {
            thread(handleClient, clientSock).detach();
        }
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}
