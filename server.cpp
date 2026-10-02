#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Guest {
    int number;
    std::string name;
    std::string phone;
    long long createdAt;
    bool called;
};

const char* const dataFile = "waitlist.tsv";

std::string trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream escaped;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': escaped << "\\\""; break;
        case '\\': escaped << "\\\\"; break;
        case '\b': escaped << "\\b"; break;
        case '\f': escaped << "\\f"; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default:
            if (ch < 0x20) {
                escaped << "\\u00" << std::hex << static_cast<int>(ch) << std::dec;
            } else {
                escaped << static_cast<char>(ch);
            }
        }
    }
    return escaped.str();
}

std::vector<Guest> loadGuests(int& autoGuestCount) {
    std::vector<Guest> guests;
    std::ifstream input(dataFile, std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string number;
        std::string name;
        std::string phone;
        std::string createdAt;
        std::string called;
        if (!std::getline(row, number, '\t')) continue;
        if (number == "AUTO") {
            try {
                if (std::getline(row, name, '\t')) autoGuestCount = std::stoi(name);
            } catch (const std::exception&) {
            }
            continue;
        }
        if (std::getline(row, name, '\t') &&
            std::getline(row, phone, '\t') && std::getline(row, createdAt, '\t')) {
            try {
                std::getline(row, called, '\t');
                guests.push_back({std::stoi(number), name, phone, std::stoll(createdAt), called == "1"});
                const std::string autoNamePrefix = "\xE8\xB7\xAF\xE4\xBA\xBA";
                const std::string autoPhonePrefix = "0900-000-";
                if (name.rfind(autoNamePrefix, 0) == 0 && phone.rfind(autoPhonePrefix, 0) == 0) {
                    autoGuestCount = std::max(autoGuestCount, std::stoi(phone.substr(autoPhonePrefix.size())));
                }
            } catch (const std::exception&) {
            }
        }
    }
    return guests;
}

bool saveGuests(const std::vector<Guest>& guests, int autoGuestCount) {
    std::ofstream output(dataFile, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << "AUTO\t" << autoGuestCount << '\n';
    for (const Guest& guest : guests) {
        output << guest.number << '\t' << guest.name << '\t' << guest.phone
             << '\t' << guest.createdAt << '\t' << (guest.called ? 1 : 0) << '\n';
    }
    return static_cast<bool>(output);
}

std::string guestsJson(const std::vector<Guest>& guests) {
    std::ostringstream json;
    json << '[';
    for (std::size_t index = 0; index < guests.size(); ++index) {
        const Guest& guest = guests[index];
        if (index != 0) json << ',';
        json << "{\"number\":" << guest.number
             << ",\"name\":\"" << jsonEscape(guest.name)
             << "\",\"phone\":\"" << jsonEscape(guest.phone)
             << "\",\"createdAt\":" << guest.createdAt
             << ",\"called\":" << (guest.called ? "true" : "false") << '}';
    }
    json << ']';
    return json.str();
}

std::string guestJson(const Guest& guest) {
    return "{\"number\":" + std::to_string(guest.number) +
           ",\"name\":\"" + jsonEscape(guest.name) +
           "\",\"phone\":\"" + jsonEscape(guest.phone) +
           "\",\"createdAt\":" + std::to_string(guest.createdAt) +
           ",\"called\":" + (guest.called ? "true}" : "false}");
}

int nextGuestNumber(const std::vector<Guest>& guests) {
    int number = 1;
    for (const Guest& guest : guests) number = std::max(number, guest.number + 1);
    return number;
}

std::string autoGuestName(int autoNumber) {
    static const char* const suffixes[] = {
        "\xE7\x94\xB2", "\xE4\xB9\x99", "\xE4\xB8\x99", "\xE4\xB8\x81", "\xE6\x88\x8A",
        "\xE5\xB7\xB1", "\xE5\xBA\x9A", "\xE8\xBE\x9B", "\xE5\xA3\xAC", "\xE7\x99\xB8"
    };
    std::string suffix;
    while (autoNumber > 0) {
        const int digit = (autoNumber - 1) % 10;
        suffix = suffixes[digit] + suffix;
        autoNumber = (autoNumber - 1) / 10;
    }
    return "\xE8\xB7\xAF\xE4\xBA\xBA" + suffix;
}

std::string autoGuestPhone(int autoNumber) {
    std::string serial = std::to_string(autoNumber);
    while (serial.size() < 3) serial.insert(serial.begin(), '0');
    return "0900-000-" + serial;
}

std::string urlDecode(const std::string& value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') {
            decoded.push_back(' ');
        } else if (value[index] == '%' && index + 2 < value.size()) {
            char hex[3] = {value[index + 1], value[index + 2], '\0'};
            char* end = nullptr;
            const long byte = std::strtol(hex, &end, 16);
            if (end != hex && *end == '\0') {
                decoded.push_back(static_cast<char>(byte));
                index += 2;
            } else {
                decoded.push_back(value[index]);
            }
        } else {
            decoded.push_back(value[index]);
        }
    }
    return decoded;
}

std::string formValue(const std::string& body, const std::string& key) {
    std::istringstream fields(body);
    std::string field;
    while (std::getline(fields, field, '&')) {
        const std::size_t separator = field.find('=');
        if (separator != std::string::npos && urlDecode(field.substr(0, separator)) == key) {
            return urlDecode(field.substr(separator + 1));
        }
    }
    return "";
}

std::string errorJson(const std::string& message) {
    return "{\"error\":\"" + jsonEscape(message) + "\"}";
}

bool sendAll(SOCKET client, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int result = send(client, data.data() + sent,
                                static_cast<int>(data.size() - sent), 0);
        if (result == SOCKET_ERROR || result == 0) return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

void respond(SOCKET client, int status, const std::string& reason,
             const std::string& contentType, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    sendAll(client, response.str());
}

void handleClient(SOCKET client, std::vector<Guest>& guests, int& autoGuestCount) {
    std::string request;
    char buffer[4096];
    std::size_t headerEnd = std::string::npos;
    while ((headerEnd = request.find("\r\n\r\n")) == std::string::npos) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0 || request.size() + static_cast<std::size_t>(received) > 65536) return;
        request.append(buffer, static_cast<std::size_t>(received));
    }

    const std::string headers = request.substr(0, headerEnd);
    std::istringstream headerStream(headers);
    std::string method;
    std::string path;
    std::string version;
    headerStream >> method >> path >> version;
    std::string headerLine;
    std::getline(headerStream, headerLine);
    std::size_t contentLength = 0;
    while (std::getline(headerStream, headerLine)) {
        const std::size_t colon = headerLine.find(':');
        if (colon != std::string::npos && lower(trim(headerLine.substr(0, colon))) == "content-length") {
            try {
                contentLength = static_cast<std::size_t>(std::stoul(trim(headerLine.substr(colon + 1))));
            } catch (const std::exception&) {
                respond(client, 400, "Bad Request", "application/json; charset=utf-8", errorJson("Invalid request"));
                return;
            }
        }
    }
    if (contentLength > 16384) {
        respond(client, 413, "Payload Too Large", "application/json; charset=utf-8", errorJson("Request is too large"));
        return;
    }

    std::string body = request.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) return;
        body.append(buffer, static_cast<std::size_t>(received));
    }
    body.resize(contentLength);

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        std::ifstream page("index.html", std::ios::binary);
        if (!page) {
            respond(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "Could not open index.html");
            return;
        }
        std::ostringstream html;
        html << page.rdbuf();
        respond(client, 200, "OK", "text/html; charset=utf-8", html.str());
    } else if (method == "GET" && path == "/api/guests") {
        respond(client, 200, "OK", "application/json; charset=utf-8", guestsJson(guests));
    } else if (method == "GET" && path == "/api/guests/next") {
        const auto next = std::find_if(guests.begin(), guests.end(), [](const Guest& guest) {
            return !guest.called;
        });
        respond(client, 200, "OK", "application/json; charset=utf-8",
                next == guests.end() ? "null" : guestJson(*next));
    } else if (method == "POST" && path == "/api/guests/call-next") {
        const auto next = std::find_if(guests.begin(), guests.end(), [](const Guest& guest) {
            return !guest.called;
        });
        if (next == guests.end()) {
            respond(client, 200, "OK", "application/json; charset=utf-8",
                    "{\"guest\":null,\"guests\":" + guestsJson(guests) + "}");
            return;
        }
        next->called = true;
        if (!saveGuests(guests, autoGuestCount)) {
            next->called = false;
            respond(client, 500, "Internal Server Error", "application/json; charset=utf-8", errorJson("Could not save waitlist"));
            return;
        }
        respond(client, 200, "OK", "application/json; charset=utf-8",
                "{\"guest\":" + guestJson(*next) + ",\"guests\":" + guestsJson(guests) + "}");
    } else if (method == "POST" && path == "/api/guests/auto") {
        const int number = nextGuestNumber(guests);
        const int autoNumber = ++autoGuestCount;
        const std::string name = autoGuestName(autoNumber);
        const std::string phone = autoGuestPhone(autoNumber);
        const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        guests.push_back({number, name, phone, now, false});
        if (!saveGuests(guests, autoGuestCount)) {
            guests.pop_back();
            --autoGuestCount;
            respond(client, 500, "Internal Server Error", "application/json; charset=utf-8", errorJson("Could not save waitlist"));
            return;
        }
        respond(client, 201, "Created", "application/json; charset=utf-8",
                "{\"number\":" + std::to_string(number) +
                ",\"name\":\"" + jsonEscape(name) +
                "\",\"phone\":\"" + jsonEscape(phone) +
                "\",\"guests\":" + guestsJson(guests) + "}");
    } else if (method == "POST" && path == "/api/guests") {
        const std::string name = trim(formValue(body, "name"));
        const std::string phone = trim(formValue(body, "phone"));
        const std::size_t digitCount = static_cast<std::size_t>(std::count_if(phone.begin(), phone.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        }));
        if (name.empty() || phone.empty() || digitCount < 8 || digitCount > 15 ||
            name.find_first_of("\t\r\n") != std::string::npos ||
            phone.find_first_of("\t\r\n") != std::string::npos) {
            respond(client, 400, "Bad Request", "application/json; charset=utf-8", errorJson("Invalid name or phone number"));
            return;
        }

        const int nextNumber = nextGuestNumber(guests);
        const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        guests.push_back({nextNumber, name, phone, now, false});
        if (!saveGuests(guests, autoGuestCount)) {
            guests.pop_back();
            respond(client, 500, "Internal Server Error", "application/json; charset=utf-8", errorJson("Could not save waitlist"));
            return;
        }
        respond(client, 201, "Created", "application/json; charset=utf-8",
            "{\"number\":" + std::to_string(nextNumber) + ",\"guests\":" + guestsJson(guests) + "}");
    } else if (method == "DELETE" && path == "/api/guests") {
        const std::vector<Guest> previous = guests;
        guests.clear();
        if (!saveGuests(guests, autoGuestCount)) {
            guests = previous;
            respond(client, 500, "Internal Server Error", "application/json; charset=utf-8", errorJson("Could not save waitlist"));
            return;
        }
        respond(client, 200, "OK", "application/json; charset=utf-8", guestsJson(guests));
    } else if (method == "DELETE" && path.rfind("/api/guests/", 0) == 0) {
        try {
            const int number = std::stoi(path.substr(std::string("/api/guests/").size()));
            const std::vector<Guest> previous = guests;
            guests.erase(std::remove_if(guests.begin(), guests.end(), [number](const Guest& guest) {
                return guest.number == number;
            }), guests.end());
            if (guests.size() == previous.size()) {
                respond(client, 404, "Not Found", "application/json; charset=utf-8", errorJson("Guest not found"));
                return;
            }
            if (!saveGuests(guests, autoGuestCount)) {
                guests = previous;
                respond(client, 500, "Internal Server Error", "application/json; charset=utf-8", errorJson("Could not save waitlist"));
                return;
            }
            respond(client, 200, "OK", "application/json; charset=utf-8", guestsJson(guests));
        } catch (const std::exception&) {
            respond(client, 400, "Bad Request", "application/json; charset=utf-8", errorJson("Invalid queue number"));
        }
    } else {
        respond(client, 404, "Not Found", "application/json; charset=utf-8", errorJson("Not found"));
    }
}

} // namespace

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception&) {
            std::cerr << "Usage: waitlist-server.exe [port]\n";
            return 1;
        }
    }
    if (port < 1 || port > 65535) {
        std::cerr << "Port must be between 1 and 65535.\n";
        return 1;
    }

    WSADATA winsockData{};
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        std::cerr << "Could not initialize Winsock.\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "Could not create server socket.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Could not listen on 127.0.0.1:" << port << ". The port may already be in use.\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    int autoGuestCount = 0;
    std::vector<Guest> guests = loadGuests(autoGuestCount);
    std::cout << "Waitlist server running at http://127.0.0.1:" << port << "/\n"
              << "Press Ctrl+C to stop.\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        handleClient(client, guests, autoGuestCount);
        shutdown(client, SD_SEND);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}