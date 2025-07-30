#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <direct.h> // For _mkdir

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int BUF = 4096;     // Buffer size for sending/receiving data
const int PORT = 12345;   // Server port to connect to
bool running = true;      // Flag to keep client running
SOCKET sockClient;        // Client socket
string myUsername;        // Username of this client

// Create directory if it doesn't exist (simple wrapper)
void ensureDirectoryExists(const string& dir) {
    _mkdir(dir.c_str());
}

// Read a file as binary and store contents into outData vector
bool readFileBinary(const string& path, vector<char>& outData) {
    ifstream f(path, ios::binary);
    if (!f) {
        cerr << "[ERROR] File not found: " << path << "\n";
        return false;
    }
    f.seekg(0, ios::end);
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    outData.resize(sz);
    f.read(outData.data(), sz);
    return true;
}

// Write binary data to file at given path
bool writeFileBinary(const string& path, const vector<char>& data) {
    ofstream f(path, ios::binary);
    if (!f) {
        cerr << "[ERROR] Failed to write file: " << path << "\n";
        return false;
    }
    f.write(data.data(), (streamsize)data.size());
    return true;
}

// Receive exactly len bytes from socket sock into buf
bool recvAll(SOCKET sock, char* buf, int len) {
    int total = 0;
    while (total < len) {
        int r = recv(sock, buf + total, len - total, 0);
        if (r <= 0) return false;  // Connection lost or error
        total += r;
    }
    return true;
}

// Thread function to continuously receive messages/files from server
void receiveLoop() {
    char buf[BUF];
    while (running) {
        int n = recv(sockClient, buf, BUF - 1, 0);
        if (n <= 0) break;  // Connection closed or error

        buf[n] = '\0';
        string msg(buf);

        // Handle incoming group file
        if (msg.rfind("FILE:", 0) == 0) {
            size_t p1 = msg.find(':');
            size_t p2 = msg.find(':', p1 + 1);
            if (p1 == string::npos || p2 == string::npos) {
                cout << msg;
                continue;
            }
            int fsz = stoi(msg.substr(p1 + 1, p2 - p1 - 1));
            string fname = msg.substr(p2 + 1);
            while (!fname.empty() && (fname.back() == '\n' || fname.back() == '\r')) fname.pop_back();

            cout << "[INFO] Receiving group file: " << fname << " (" << fsz << " bytes)\n";

            vector<char> data(fsz);
            if (!recvAll(sockClient, data.data(), fsz)) {
                cout << "[ERROR] Connection lost while receiving file\n";
                running = false;
                break;
            }

            ensureDirectoryExists("GroupMedias");  // Create folder if not exists
            string path = "GroupMedias\\" + fname;

            if (writeFileBinary(path, data)) {
                cout << "[INFO] Group file saved: " << path << "\n";
            }
            else {
                cout << "[ERROR] Failed to save group file\n";
            }
        }
        // Handle private messages and private file incoming
        else if (!msg.empty() && msg[0] == '@') {
            size_t sp = msg.find(' ');
            if (sp == string::npos) {
                cout << msg;
                continue;
            }
            string sender = msg.substr(1, sp - 1);
            string rest = msg.substr(sp + 1);

            // Private file incoming
            if (rest.rfind("FILE:", 0) == 0) {
                size_t p1 = rest.find(':');
                size_t p2 = rest.find(':', p1 + 1);
                if (p1 == string::npos || p2 == string::npos) {
                    cout << msg;
                    continue;
                }
                int fsz = stoi(rest.substr(p1 + 1, p2 - p1 - 1));
                string fname = rest.substr(p2 + 1);
                while (!fname.empty() && (fname.back() == '\n' || fname.back() == '\r')) fname.pop_back();

                cout << "[INFO] Receiving private file from " << sender << ": " << fname << " (" << fsz << " bytes)\n";

                vector<char> data(fsz);
                if (!recvAll(sockClient, data.data(), fsz)) {
                    cout << "[ERROR] Connection lost while receiving private file\n";
                    running = false;
                    break;
                }

                // Save private file under Medias/{SenderName}/
                string userDir = "Medias\\" + sender;
                ensureDirectoryExists("Medias");
                ensureDirectoryExists(userDir);

                string path = userDir + "\\" + fname;
                if (writeFileBinary(path, data)) {
                    cout << "[INFO] Private file saved: " << path << "\n";
                }
                else {
                    cout << "[ERROR] Failed to save private file\n";
                }
            }
            else {
                // Normal private text message, just print
                cout << msg;
            }
        }
        else {
            // Normal group message, just print
            cout << msg;
        }
    }
    running = false; // Mark client as stopped if connection lost
}

// Thread function to read user input and send messages/files to server
void sendLoop() {
    string msg;
    while (running && getline(cin, msg)) {
        // Handle sending group file with /sendfile <path>
        if (msg.rfind("/sendfile ", 0) == 0) {
            string filepath = msg.substr(10);
            if (!filepath.empty() && filepath.front() == '"' && filepath.back() == '"')
                filepath = filepath.substr(1, filepath.size() - 2);

            vector<char> data;
            if (!readFileBinary(filepath, data)) continue;

            string filename;
            size_t pos = filepath.find_last_of("\\/");
            if (pos != string::npos) filename = filepath.substr(pos + 1);
            else filename = filepath;

            // Send header: FILE:<size>:<filename>\n
            string header = "FILE:" + to_string(data.size()) + ":" + filename + "\n";
            send(sockClient, header.c_str(), (int)header.size(), 0);

            // Send file data in chunks
            int sent = 0;
            while (sent < (int)data.size()) {
                int chunk = min((int)data.size() - sent, BUF);
                int r = send(sockClient, data.data() + sent, chunk, 0);
                if (r == SOCKET_ERROR) break;
                sent += r;
            }
            cout << "[INFO] Group file sent: " << filepath << "\n";
        }
        // Handle private messages or private file send (starts with '@username ')
        else if (!msg.empty() && msg[0] == '@') {
            size_t sp = msg.find(' ');
            if (sp == string::npos) {
                send(sockClient, msg.c_str(), (int)msg.size(), 0);
                continue;
            }
            string target = msg.substr(1, sp - 1);
            string rest = msg.substr(sp + 1);

            // Detect if rest looks like a file path by checking slashes
            if (rest.find('/') != string::npos || rest.find('\\') != string::npos) {
                string filepath = rest;
                if (!filepath.empty() && filepath.front() == '"' && filepath.back() == '"')
                    filepath = filepath.substr(1, filepath.size() - 2);

                vector<char> data;
                if (!readFileBinary(filepath, data)) continue;

                string filename;
                size_t pos = filepath.find_last_of("\\/");
                if (pos != string::npos) filename = filepath.substr(pos + 1);
                else filename = filepath;

                // Send header with private file info: @username FILE:<size>:<filename>\n
                string header = "@" + myUsername + " FILE:" + to_string(data.size()) + ":" + filename + "\n";
                send(sockClient, header.c_str(), (int)header.size(), 0);

                // Send file data in chunks
                int sent = 0;
                while (sent < (int)data.size()) {
                    int chunk = min((int)data.size() - sent, BUF);
                    int r = send(sockClient, data.data() + sent, chunk, 0);
                    if (r == SOCKET_ERROR) break;
                    sent += r;
                }
                cout << "[INFO] Private file sent to " << target << ": " << filepath << "\n";
            }
            else {
                // Just send normal private text message
                send(sockClient, msg.c_str(), (int)msg.size(), 0);
            }
        }
        // Handle quit command
        else if (msg == "/quit") {
            send(sockClient, msg.c_str(), (int)msg.size(), 0);
            running = false;
            break;
        }
        else {
            // Send normal group text message
            send(sockClient, msg.c_str(), (int)msg.size(), 0);
        }
    }
}

int main() {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    // Create TCP socket
    sockClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockClient == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // Set up server address (localhost:12345)
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(sockClient, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "Connection failed: " << WSAGetLastError() << "\n";
        closesocket(sockClient);
        WSACleanup();
        return 1;
    }

    cout << "Enter your username: ";
    getline(cin, myUsername);

    // Send username to server as first message
    send(sockClient, myUsername.c_str(), (int)myUsername.size(), 0);

    // Display instructions to user
    cout << "\nConnected!\n"
        << "- Type message for public chat\n"
        << "- Use @username <message> for private message\n"
        << "- Use @username <path> to send private file\n"
        << "- Use /sendfile <path> to send group file\n"
        << "- Type /list to view active users\n"
        << "- Type /quit to exit\n\n";

    // Start receiving and sending threads
    thread tRecv(receiveLoop);
    thread tSend(sendLoop);

    // Wait for threads to finish
    tRecv.join();
    tSend.join();

    // Cleanup socket and Winsock
    closesocket(sockClient);
    WSACleanup();
    return 0;
}
