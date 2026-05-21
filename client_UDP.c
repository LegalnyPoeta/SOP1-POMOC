/**
 * Klient UDP - Nadawca plików.
 * Program dzieli plik na mniejsze paczki i wysyła je po UDP do serwera.
 * Implementuje mechanizm Stop-and-Wait z retransmisją: po wysłaniu paczki
 * oczekuje 0.5s na potwierdzenie. Jeśli go nie otrzyma, wysyła paczkę ponownie.
 */

#include "l8_common.h"

#define MAXBUF 576 // Maksymalny rozmiar wysyłanego datagramu

// Zmienna globalna do zapamiętywania ostatnio odebranego sygnału.
// Musi być typu sig_atomic_t dla bezpieczeństwa operacji asynchronicznych.
volatile sig_atomic_t last_signal = 0;

// Handler sygnału wywoływany, gdy minie czas timeoutu
void sigalrm_handler(int sig) { last_signal = sig; }

int make_socket() {
    int sock;
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) ERR("socket");
    return sock;
}

void usage(char *name) { 
    fprintf(stderr, "USAGE: %s domain port file \n", name); 
}

/* Funkcja odpowiedzialna za wysłanie paczki i oczekiwanie na potwierdzenie (ACK) z uwzględnieniem timeoutu. */
void sendAndConfirm(int fd, struct sockaddr_in addr, char *sendbuf, char *recvbuf, ssize_t size) {
    struct itimerval ts;
    
    // 1. Wysłanie pakietu w ciemno (UDP)
    if (TEMP_FAILURE_RETRY(sendto(fd, sendbuf, size, 0, (struct sockaddr *)&addr, sizeof(addr))) < 0)
        ERR("sendto:");

    // 2. Konfiguracja timera na 500ms (0.5 sekundy)
    memset(&ts, 0, sizeof(struct itimerval));
    ts.it_value.tv_usec = 500000;
    setitimer(ITIMER_REAL, &ts, NULL);
    last_signal = 0; // Zerowanie flagi przed wejściem w nasłuch

    // 3. Blokujące oczekiwanie na odpowiedź.
    // UWAGA: Nie używamy TEMP_FAILURE_RETRY! Inaczej sygnał SIGALRM przerwałby funkcję,
    // a makro automatycznie wywołałoby recv() ponownie, ignorując nasz timeout.
    while (recv(fd, recvbuf, size, 0) < 0) {
        if (EINTR != errno) ERR("recv:");      // Jeśli to inny błąd niż przerwanie sygnałem, rzuć błędem
        if (SIGALRM == last_signal) break;     // Jeśli uderzył nasz timer, przerwij pętlę i wróć
    }
}

void doClient(int fd, struct sockaddr_in addr, int file) {
    char sendbuf[MAXBUF];
    char recvbuf[MAXBUF];
    int offset = 2 * sizeof(int32_t); // 8 bajtów na metadane: [nr_paczki][flaga_konca]
    int32_t chunkNo = 0;
    ssize_t size;
    int counter;

    do {
        memset(sendbuf, 0, MAXBUF);
        memset(recvbuf, 0, MAXBUF);
        
        // Wczytywanie surowych danych z pliku pomijając miejsce na metadane (offset)
        if ((size = bulk_read(file, sendbuf + offset, MAXBUF - offset)) < 0)
            ERR("read from file:");

        // Konwersja numeru paczki na Network Byte Order i wpisanie na pierwsze 4 bajty
        *((int32_t *)sendbuf) = htonl(++chunkNo);

        // Jeśli wczytano mniej bajtów niż wynosi miejsce w buforze -> to końcówka pliku
        if (size < MAXBUF - offset) {
            memset(sendbuf + offset + size, 0, MAXBUF - offset - size);
            // Ustawienie flagi końca pliku na drugich 4 bajtach
            *(((int32_t *)sendbuf) + 1) = htonl(1);
        }

        counter = 0;
        
        // Pętla Stop-and-Wait (Retransmisja)
        do {
            counter++;
            sendAndConfirm(fd, addr, sendbuf, recvbuf, MAXBUF);
            
            // Warunek kontynuacji prób:
            // 1. Oczekujemy w potwierdzeniu naszego numeru paczki (skonwertowanego do Network Order).
            //    Jeśli numery się różnią (lub przyszły zera przez timeout), próbujemy ponownie.
            // 2. Maksymalna liczba prób to 5.
        } while (*((int32_t *)recvbuf) != (int32_t)htonl(chunkNo) && counter <= 5);

        // Jeśli po 5 próbach nadal nie ma odpowiedniego potwierdzenia, przerywamy działanie
        if (*((int32_t *)recvbuf) != (int32_t)htonl(chunkNo) && counter > 5)
            break;

    } while (size == MAXBUF - offset); // Kontynuuj dopóki czytamy pełne porcje pliku
}

int main(int argc, char **argv) {
    int fd, file;
    struct sockaddr_in addr;

    if (argc != 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Konfiguracja obsługi sygnałów
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Seting SIGPIPE:");
    
    // Rejestracja handlera dla timera odpowiedzialnego za zrywanie recv()
    if (sethandler(sigalrm_handler, SIGALRM)) ERR("Seting SIGALRM:");

    if ((file = TEMP_FAILURE_RETRY(open(argv[3], O_RDONLY))) < 0) ERR("open:");
    
    fd = make_socket();
    addr = make_address(argv[1], argv[2]); // Zbudowanie struktury adresowej serwera
    
    doClient(fd, addr, file);
    
    if (TEMP_FAILURE_RETRY(close(fd)) < 0) ERR("close");
    if (TEMP_FAILURE_RETRY(close(file)) < 0) ERR("close");
    
    return EXIT_SUCCESS;
}