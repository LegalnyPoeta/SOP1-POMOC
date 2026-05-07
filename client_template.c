#include "common.h"

// Funkcja laczaca sie po TCP przy uzyciu nazwy domenowej (getaddrinfo)
int connect_tcp(const char *host, const char *port) {
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret;
    if ((ret = getaddrinfo(host, port, &hints, &result))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) ERR("socket");

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) ERR("connect");

    freeaddrinfo(result);
    return sock;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uzycie: %s <host> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int sock = connect_tcp(argv[1], argv[2]);
    printf("Polaczono z serwerem!\n");

    // Przyklad przesylania struktur: 
    // Zawsze wysylajac liczby zrob htonl/htons, 
    // a po odbiorze ntohl/ntohs.
    // bulk_write(sock, dane, rozmiar);
    // bulk_read(sock, bufor_na_odpowiedz, rozmiar);

    close(sock);
    return EXIT_SUCCESS;
}