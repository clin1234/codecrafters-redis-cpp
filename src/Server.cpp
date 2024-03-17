#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <atomic>

void recv_loop(int client_fd) {
  std::cout << "Client connected\n";
  char buffer[1024];      // Buffer to store received data
  ssize_t bytes_received; // Variable to store the number of bytes received

  while ((bytes_received = recv(client_fd, buffer, sizeof(buffer), 0)) >
         0) { // Receive data in a loop
    send(client_fd, "+PONG\r\n", 7,
         0); // Respond with +PONG\r\n for each received command
  }
  //close(fd);
}


std::mutex threads_mutex;
std::vector <std::jthread> thread_vector;

void add_thread(std::jthread&& thread) {
  std::lock_guard<std::mutex> guard(threads_mutex);
  thread_vector.push_back(std::move(thread));
}


void handle_connection(int client_fd) {
  char recv_buffer[1024];
  std::string_view pong = "+PONG\r\n";
  while (recv(client_fd, recv_buffer, sizeof(recv_buffer), 0)) {
    send(client_fd, pong.c_str(), pong.length(), 0);
  }
  close(client_fd);
}

int main(int argc, char **argv) {
  // You can use print statements as follows for debugging, they'll be visible
  // when running tests. std::cout << "Logs from your program will appear
  // here!\n";

  // Uncomment this block to pass the first stage

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
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
  int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                  (socklen_t *)&client_addr_len);
  if (client_fd > 0) {
std::jthread client_connection(handle_connection, client_fd);
      add_thread(std::move(client_connection));
}

 // std::jthread r1(recv_loop, client_fd);
  //std::jthread r2(recv_loop, client_fd);

  // int fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *)
  // &client_addr_len); std::cout << "Client connected\n";

  close(client_fd);
  close(server_fd);

  return 0;
}
