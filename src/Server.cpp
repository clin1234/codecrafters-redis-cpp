#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cstddef>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <cctype>
#include <functional>
#include <any>
#include <type_traits>
#include <charconv>
#include <variant>

constexpr unsigned BUFFER_SIZE = 1024;

using namespace std::literals;

struct string_hash
{
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(const char* str) const { return hash_type{}(str); }
    std::size_t operator()(std::string_view str) const { return hash_type{}(str); }
    std::size_t operator()(std::string const& str) const { return hash_type{}(str); }
};

enum commands : unsigned char {
    ping, echo
};

void parse_cmd(std::string_view cmd) {
    const std::unordered_map<std::string, commands, string_hash, std::equal_to<>> to_enum{
        {"ping"sv, ping},
        {"echo"sv, echo}
    };

    if (auto e = to_enum.find(cmd); e != to_enum.end()) {
        auto func = response_map[e->second];
        auto return_type = func(cmd);
        if constexpr (std::is_same_v<return_type, std::string>);
        else if constexpr (std::is_same_v<return_type, std::vector<std::string>>);
    }
}

// XXX: assumes inputs are valid and sanitized prior

std::string decode_simple_string(std::string_view cmd) {
    cmd.remove_prefix(1); // '+'
    cmd.remove_suffix(2); // "\r\n"
    return std::string(cmd);
}

std::vector<std::string> decode_array_string(std::string_view cmd) {
    cmd.remove_prefix(1); // '*'
    std::size_t pos_1st_r = cmd.find_first_of('\r');
    std::size_t pos_1st_n = cmd.find_first_of('\n');
    unsigned n_elements{};
    std::from_chars(cmd.data(), cmd.data() + pos_1st_r - 1, n_elements);

    cmd.remove_prefix(pos_1st_n); // Shift view to begin at 1st character after 1st '\n'

    std::vector<std::string> result(n_elements);

    std::size_t tmp_r;
    for (auto i = 0; i < n_elements; i++) {
        tmp_r = cmd.find_first_of('\r');
        result.emplace_back(cmd.substr(0, tmp_r));
        cmd.remove_prefix(tmp_r + 1);
    }

    return result;
}

std::string decode_bulk_string(std::string_view cmd) {
    cmd.remove_prefix(1); // '$'
    cmd.remove_suffix(2); // 2nd "\r\n"
    std::size_t pos_1st_r = cmd.find_first_of('\r');
    std::size_t pos_1st_n = cmd.find_first_of('\n');
    // static_assert(cmd[pos_1st_rn] != '\r' || cmd[pos_1st_rn] != '\n');
    unsigned size{};
    std::from_chars(cmd.data(), cmd.data()+pos_1st_r-1, size);
    return std::string(cmd, pos_1st_n+1, size);
}

using decoding_func = std::function<std::variant<std::string,std::vector<std::string>>(std::string_view)>;

const std::unordered_map<commands, decoding_func> response_map{
        {ping, decode_simple_string},
        {echo, decode_array_string}
};

void handleClient(int client_fd)
{
    while (true) {
        char command[BUFFER_SIZE]{};
        // std::cout << "Client is: " << client_fd << '\n';
        ssize_t received = recv(client_fd, command, sizeof(command), 0);
        if (received == -1) {
            std::cerr << "[" << client_fd << "]" << "recv err: " << strerror(errno) << '\n';
            break;
        }
        else if (received == 0) {
            std::cout << "Client " << client_fd << " connection closed.\n";
            break;
        }   
        else {
            for (unsigned i = 0; i < BUFFER_SIZE; i++) command[i] = std::tolower(command[i]);
            parse_cmd(command);
            const char pong[] = "+PONG\r\n";
            ssize_t bytes_sent = send(client_fd, pong, strlen(pong), 0);
            if (bytes_sent < 0) {
                std::cerr << "[" << client_fd << "]" << " send err: " << strerror(errno) << '\n';
                break;
            }
        }
    }
    close(client_fd);
}

int main(int argc, char **argv) {
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  // std::cout << "Logs from your program will appear here!\n";
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting REUSE_PORT
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  std::vector<std::jthread> client_threads;
  
  while (true)
  {
      int conn_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
      if (conn_fd >= 0) {
          // std::cout << "Client connected: " << conn_fd << std::endl;
          client_threads.emplace_back(std::jthread(handleClient, conn_fd));
          client_threads.back().detach();
      }
  }
  
  close(server_fd);
  return 0;
}