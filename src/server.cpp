#include "server_utils.h"
#include "config.h"
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

static std::string url_decode(const std::string& s) 
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) 
    {
        if (s[i] == '%') 
        {
            if (i + 2 < s.size()) 
            {
                int value = 0;
                std::istringstream is(s.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    out += static_cast<char>(value);
                    i += 2;
                }
            }
        }
        else if (s[i] == '+') 
        {
            out += ' ';
        }
        else out += s[i];
    }
    return out;
}

static std::string build_page(const std::vector<std::string>& results, const std::string& query) 
{
    std::ostringstream ss;
    ss << "<!doctype html><html><head><meta charset='utf-8'><title>Search</title></head><body>";
    ss << "<h3>Search</h3>";
    ss << "<form method='GET' action='/'><input name='q' value='" << query << "' size='60'/><input type='submit' value='Search'/></form>";
    ss << "<hr/>";
    ss << "<div>Results (" << results.size() << ")</div><ul>";
    for (auto& r : results) 
    {
        ss << "<li><a href='" << r << "'>" << r << "</a></li>";
    }
    ss << "</ul></body></html>";
    return ss.str();
}

void handle_client(SOCKET client_socket) 
{
    const int BUFSIZE = 8192;
    char buffer[BUFSIZE];

    int read_bytes = recv(client_socket, buffer, BUFSIZE - 1, 0);
    if (read_bytes <= 0) 
    {
        closesocket(client_socket);
        return;
    }
    buffer[read_bytes] = 0;

    std::string req(buffer);
    std::istringstream reqs(req);
    std::string method, path, proto;
    reqs >> method >> path >> proto;

    std::string query;
    std::string qval;
    auto pos = path.find('?');
    if (pos != std::string::npos) 
    {
        query = path.substr(pos + 1);
        path = path.substr(0, pos);
        size_t p = query.find("q=");
        if (p != std::string::npos) 
        {
            qval = query.substr(p + 2);
            auto amp = qval.find('&');
            if (amp != std::string::npos) qval = qval.substr(0, amp);
            qval = url_decode(qval);
        }
    }

    std::vector<std::string> results;
    if (!qval.empty()) results = search_pages(qval);
    else results = {};

    std::string body = build_page(results, qval);
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n" << body;
    auto s = resp.str();

    send(client_socket, s.c_str(), s.size(), 0);
    closesocket(client_socket);
}

int main() 
{
    Config cfg = load_config("config.ini");
    int port = cfg.get_int("server.port", 8080);

    // Инициализация Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) 
    {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) 
    {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Устанавливаем опцию переиспользования адреса
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) 
    {
        std::cerr << "setsockopt failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_socket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) 
    {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, 10) == SOCKET_ERROR) 
    {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (true) 
    {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) 
        {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        std::thread(handle_client, client_socket).detach();
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}