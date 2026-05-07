#include "common.h"

#define MAX_EVENTS 10
#define BACKLOG 5

volatile sig_atomic_t do_work = 1;
void sigint_handler(int sig) { do_work = 0; } // Aby przerwac dzialanie CTRL+C

// Konfiguracja gniazda TCP do nasluchiwania
int bind_tcp_listen(uint16_t port) {
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) ERR("socket");

    int t = 1;
    // Zapobiega "Address already in use" przy restartach
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t))) ERR("setsockopt");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) ERR("bind");
    if (listen(sock, BACKLOG) < 0) ERR("listen");
    
    set_nonblock(sock); // Bardzo wazne na labach z epoll
    return sock;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uzycie: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // 1. Zabezpieczenia (SIGPIPE i CTRL+C)
    signal(SIGPIPE, SIG_IGN); 
    
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);

    // 2. Setup Gniazda i epoll
    int listen_sock = bind_tcp_listen(atoi(argv[1]));
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) ERR("epoll_create1");

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN; // Chcemy byc budzeni gdy mozna cos "Odczytac"
    event.data.fd = listen_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event) == -1) ERR("epoll_ctl");

    printf("Serwer wystartowal...\n");

    // 3. Glowna petla
    while (do_work) {
        // Czekamy na zdarzenia
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue; // Jesli przerwane przez ctrl+c to OK
            ERR("epoll_wait");
        }

        // Analizujemy kto nas obudzil
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_sock) {
                // Ktos chce sie polaczyc!
                int client_sock = TEMP_FAILURE_RETRY(accept(listen_sock, NULL, NULL));
                if (client_sock < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                    ERR("accept");
                }
                
                // Dodajemy nowego klienta do epolla
                event.events = EPOLLIN;
                event.data.fd = client_sock;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &event);
                printf("Nowy klient podlaczony!\n");
            } else {
                // To stary klient cos wyslal!
                int client = events[i].data.fd;
                int32_t buf[5]; // Przykladowy bufor - pamietaj o ntohl/htonl!

                // Czytamy GWARANTOWANIE
                ssize_t bytes_read = bulk_read(client, (char*)buf, sizeof(buf));
                
                if (bytes_read <= 0) {
                    // Klient sie rozlaczyl lub wystapil blad
                    printf("Klient rozlaczony.\n");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, NULL);
                    close(client);
                } else {
                    // TU ZNAJDUJE SIE LOGIKA (np. odpowiedz)
                    // np. bulk_write(client, odpowiedz, rozmiar);
                }
            }
        }
    }

    close(listen_sock);
    close(epoll_fd);
    printf("Serwer zatrzymany.\n");
    return EXIT_SUCCESS;
}