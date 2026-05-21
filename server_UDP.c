/**
 * Serwer UDP - Odbiorca plików.
 * Program nasłuchuje na podanym porcie, odbiera pakiety danych od klientów,
 * składa je w odpowiedniej kolejności i wypisuje na standardowe wyjście.
 * Obsługuje do 5 równoległych "połączeń" i wysyła potwierdzenia (ACK) po każdym pakiecie.
 */

#include "l8_common.h"

#define BACKLOG 3
#define MAXBUF 576  // Maksymalny rozmiar datagramu (metadane + dane)
#define MAXADDR 5   // Maksymalna liczba jednocześnie obsługiwanych klientów

/* Struktura przechowująca stan "połączenia" dla danego klienta.
 * Mimo że UDP jest bezpołączeniowe, musimy pamiętać, jakiej paczki 
 * oczekujemy od konkretnego nadawcy. */
struct connections {
    int free;               // 1 = slot wolny, 0 = slot zajęty
    int32_t chunkNo;        // Numer ostatnio poprawnie odebranej paczki
    struct sockaddr_in addr; // Adres IP i port klienta (identyfikator)
};

int make_socket(int domain, int type) {
    int sock;
    sock = socket(domain, type, 0);
    if (sock < 0) ERR("socket");
    return sock;
}

int bind_inet_socket(uint16_t port, int type) {
    struct sockaddr_in addr;
    int socketfd, t = 1;

    socketfd = make_socket(PF_INET, type);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Nasłuchuj na wszystkich interfejsach

    // Zabezpieczenie przed blokowaniem portu po restarcie serwera
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");
        
    if (SOCK_STREAM == type)
        if (listen(socketfd, BACKLOG) < 0) ERR("listen");
        
    return socketfd;
}

/* Funkcja znajdująca klienta w tablicy połączeń na podstawie jego adresu (IP + Port).
 * Jeśli to nowy klient, przydziela mu pierwszy wolny slot w tablicy. */
int findIndex(struct sockaddr_in addr, struct connections con[MAXADDR]) {
    int i, empty = -1, pos = -1;
    for (i = 0; i < MAXADDR; i++) {
        if (con[i].free) {
            empty = i; // Zapamiętujemy wolny slot na wypadek nowego klienta
        }
        // Porównanie struktur adresowych (memcmp) z dokładnością do bajta
        else if (0 == memcmp(&addr, &(con[i].addr), sizeof(struct sockaddr_in))) {
            pos = i;
            break; // Znaleziono powracającego klienta
        }
    }
    
    // Jeśli nie znaleźliśmy klienta, ale jest wolne miejsce - dodajemy go
    if (-1 == pos && empty != -1) {
        con[empty].free = 0;
        con[empty].chunkNo = 0; // Oczekujemy paczki nr 1
        con[empty].addr = addr;
        pos = empty;
    }
    return pos;
}

void doServer(int fd) {
    struct sockaddr_in addr;
    struct connections con[MAXADDR];
    char buf[MAXBUF + 1];

    // Inicjalizacja tablicy połączeń (wszystkie sloty wolne)
    for (int i = 0; i < MAXADDR; i++)
        con[i].free = 1;

    while (1) {
        socklen_t size = sizeof(addr);
        int receivedBytes;

        // Blokujące oczekiwanie na JAKIKOLWIEK pakiet UDP
        if ((receivedBytes = TEMP_FAILURE_RETRY(recvfrom(fd, buf, MAXBUF, 0, (struct sockaddr *)&addr, &size))) < 0)
            ERR("read:");
            
        buf[receivedBytes] = 0;
        int index = -1;

        if ((index = findIndex(addr, con)) >= 0) {
            // Zamiana Network Byte Order na Host Byte Order dla pierwszych 4 bajtów (numer paczki)
            int32_t chunkNo = ntohl(*((int32_t *)buf));

            // Logika odporności na błędy UDP (Stop-and-Wait)
            if (chunkNo > con[index].chunkNo + 1) {
                // Pakiet "z przyszłości" - zgubiliśmy poprzedni, więc ten ignorujemy.
                // Klient nie dostanie potwierdzenia i wyśle zagubiony jeszcze raz.
                continue;
            } 
            else if (chunkNo == con[index].chunkNo + 1) {
                // Pakiet idealny (kolejny oczekiwany)
                
                // Sprawdzenie flagi końca pliku (kolejne 4 bajty metadanych)
                if (ntohl(*(((int32_t *)buf) + 1))) {
                    printf("Last Part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t));
                    con[index].free = 1; // Zwalniamy slot klienta, transmisja zakończona
                } else {
                    printf("Part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t));
                }
                con[index].chunkNo++; // Przesuwamy wskaźnik oczekiwanej paczki
            }
            // UWAGA: Jeśli chunkNo < oczekiwany, to jest to DUPLIKAT. 
            // Ignorujemy logikę wypisywania, ale idziemy dalej, by wysłać ponowne ACK.

            // Wysłanie potwierdzenia (ACK) do klienta. 
            // Odsyłamy mu jego własny bufor, w którym są oryginalne metadane.
            if (TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUF, 0, (struct sockaddr *)&addr, size)) < 0) {
                if (EPIPE == errno)
                    con[index].free = 1;
                else
                    ERR("send:");
            }
        }
    }
}

void usage(char *name) { 
    fprintf(stderr, "USAGE: %s port\n", name); 
}

int main(int argc, char **argv) {
    int fd;
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Ignorowanie SIGPIPE, zapobiega zamknięciu serwera przy przerwaniu połączenia
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Seting SIGPIPE:");
    
    fd = bind_inet_socket(atoi(argv[1]), SOCK_DGRAM);
    doServer(fd);
    
    if (TEMP_FAILURE_RETRY(close(fd)) < 0) ERR("close");
    
    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}