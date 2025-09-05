#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/socket.h>

#define CLEAR  "\x1b[2k\r"
#define PURPLE "\x1b[1;35m"
#define BLUE   "\x1b[1;34m"
#define length(array) (sizeof(array) / sizeof((array)[0]))

static struct kevent64_s events[1024] = {};

static char packet[65535];

static int parse_port(char* input) {
  int output = 0;
  for (char* i = input; *i != 0; i++) {
    int c = *i;
    assert('0' <= c && c <= '9');
    output = 10 * output + *i - '0';
    assert(output <= 65535);
  }
  return htons(output);
}

static struct kevent64_s make_filter(int fd) {
  return (struct kevent64_s) { .ident = fd, .filter = EVFILT_READ, .flags = EV_ADD };
}

int main(int argc, char** argv) {
  if (argc != 4) {
    puts("Usage: chat SOURCE_PORT PEER_ADDRESS PEER_PORT");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in peer = {
    .sin_family = AF_INET,
    .sin_addr   = inet_addr(argv[2]),
    .sin_port   = parse_port(argv[3]),
  };
  assert(peer.sin_addr.s_addr != INADDR_NONE);
  
  int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  assert(socket_fd != -1);

  struct sockaddr_in wildcard = {
    .sin_family      = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_ANY),
    .sin_port        = parse_port(argv[1]),
  };
  assert(bind(socket_fd, (struct sockaddr*) &wildcard, sizeof(wildcard)) == 0);
  
  int kqueue_fd = kqueue();
  assert(kqueue_fd != -1);

  struct kevent64_s filters[] = { make_filter(STDIN_FILENO), make_filter(socket_fd) };
  assert(kevent64(kqueue_fd, filters, length(filters), NULL, 0, 0, NULL) == 0);
                  
  while (1) {
    fputs(PURPLE "⭆ ", stdout);
    fflush(stdout);

    int event_count = kevent64(kqueue_fd, NULL, 0, events, length(events), 0, NULL);
    for (int i = 0; i < event_count; i++) {
      if (events[i].ident == STDIN_FILENO) {
        ssize_t packet_size = read(STDIN_FILENO, packet, sizeof(packet));
        assert(packet_size != -1);
        ssize_t sent = sendto(
          socket_fd, packet, packet_size, 0, (struct sockaddr*) &peer, sizeof(peer)
        );
        assert(sent == packet_size);
      } else if (events[i].ident == socket_fd) {
        ssize_t packet_size = recv(socket_fd, &packet, sizeof(packet), 0);
        assert(packet_size != -1);
        fputs(CLEAR BLUE "⭅ ", stdout);
        fwrite(packet, 1, packet_size, stdout);
      }
    }
  }
}
