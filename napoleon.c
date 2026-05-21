#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>

#define MAXBUF 576

// ==========================================
// ETAP 2: Konfiguracja puli wątków i stosu
// ==========================================
#define STACK_SIZE 16
#define NUM_ADJUTANTS 4

#define ERR(source) (perror(source), fprintf(stderr, "Error at %s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// ETAP 2: Struktury danych dla stosu raportów
struct stack_element {
    char data[MAXBUF + 1];
    ssize_t len;
};

struct report_stack {
    struct stack_element items[STACK_SIZE];
    int top;
    pthread_mutex_t mutex; // ETAP 2: Mutex do ochrony dostępu do stosu
    sem_t sem_filled;      // ETAP 2: Semafor zliczający elementy na stosie
};

struct report_stack stack;

// ==========================================
// ETAP 2: Funkcja robocza adiutanta (wątku)
// ==========================================
void *adjutant_work(void *arg)
{
    struct stack_element current_item;

    while (1)
    {
        // ETAP 2: Czekanie na semaforze, aż na stosie pojawi się meldunek
        if (sem_wait(&stack.sem_filled) != 0) {
            if (errno == EINTR) continue;
            ERR("sem_wait");
        }

        // ETAP 2: Pobieranie meldunku ze stosu (zabezpieczone mutexem)
        pthread_mutex_lock(&stack.mutex);
        current_item = stack.items[stack.top];
        stack.top--;
        pthread_mutex_unlock(&stack.mutex);

        // ==========================================
        // ETAP 1: Parsowanie i walidacja wiadomości
        // (W Etapie 1 robił to serwer, w Etapie 2 robi to adiutant)
        // ==========================================
        int x, y, p;
        char name[129];

        // Format datagramu: <X> <Y> <P> <nazwa oddziału>
        int parsed = sscanf(current_item.data, "%d %d %d %128s", &x, &y, &p, name);

        // ETAP 1: Obsługa błędów formatowania (wypisanie błędu bez kończenia programu)
        if (parsed != 4)
        {
            fprintf(stderr, "Blad [Adiutant]: Niepoprawny format wiadomosci.\n");
            continue;
        }
        if (x < 0 || x > 99 || y < 0 || y > 99)
        {
            fprintf(stderr, "Blad [Adiutant]: Wspolrzedne poza zakresem 0-99 (X: %d, Y: %d).\n", x, y);
            continue;
        }
        if (p != 0 && p != 1)
        {
            fprintf(stderr, "Blad [Adiutant]: Niepoprawna przynaleznosc oddzialu (P: %d).\n", p);
            continue;
        }

        // ETAP 1: Wypisanie sformatowanego raportu
        const char *side = (p == 1) ? "Nasz" : "Wrogi";
        printf("%s oddzial %s byl widziany na pozycji %d:%d.\n", side, name, x, y);
        fflush(stdout);
    }

    return NULL;
}

// ==========================================
// ETAP 1: Inicjalizacja gniazda UDP
// ==========================================
int make_socket(int domain, int type)
{
    int sock = socket(domain, type, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

int bind_inet_socket(uint16_t port, int type)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;

    socketfd = make_socket(PF_INET, type); // Tworzenie gniazda UDP (SOCK_DGRAM)

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind");

    return socketfd;
}

// ==========================================
// ETAP 1 & 2: Główna pętla odbierająca pakiety
// ==========================================
void doServer(int fd)
{
    struct sockaddr_in addr;
    char buf[MAXBUF + 1];

    while (1)
    {
        socklen_t size = sizeof(addr);
        ssize_t receivedBytes;

        // ETAP 1: Odbieranie datagramu przez recvfrom
        receivedBytes = TEMP_FAILURE_RETRY(recvfrom(fd, buf, MAXBUF, 0, (struct sockaddr *)&addr, &size));
        if (receivedBytes < 0)
            ERR("recvfrom");

        buf[receivedBytes] = '\0';

        // ETAP 2: Zamiast od razu parsować, wrzucamy na stos dla adiutantów
        pthread_mutex_lock(&stack.mutex);

        if (stack.top < STACK_SIZE - 1)
        {
            stack.top++;
            memcpy(stack.items[stack.top].data, buf, receivedBytes + 1);
            stack.items[stack.top].len = receivedBytes;
            pthread_mutex_unlock(&stack.mutex);
            
            // ETAP 2: Powiadomienie adiutantów, że jest nowa praca
            sem_post(&stack.sem_filled); 
        }
        else
        {
            pthread_mutex_unlock(&stack.mutex);
            fprintf(stderr, "Blad [Sztab]: Stos raportow jest pelny! Meldunek odrzucony.\n");
        }
    }
}

int main(int argc, char **argv) {
    int fd;
    pthread_t adjutants[NUM_ADJUTANTS]; // ETAP 2: Tablica wątków

    // ETAP 1: Pobieranie portu z argumentów wywołania
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Blad: Niepoprawny numer portu.\n");
        return EXIT_FAILURE;
    }

    // ETAP 2: Inicjalizacja stosu i mechanizmów synchronizacji
    stack.top = -1;
    if (pthread_mutex_init(&stack.mutex, NULL) != 0)
        ERR("pthread_mutex_init");
    if (sem_init(&stack.sem_filled, 0, 0) != 0) // Początkowo 0 elementów na stosie
        ERR("sem_init");

    // ETAP 2: Uruchomienie 4 wątków adiutantów
    for (int i = 0; i < NUM_ADJUTANTS; i++)
    {
        if (pthread_create(&adjutants[i], NULL, adjutant_work, NULL) != 0)
            ERR("pthread_create");
    }

    // ETAP 1: Start serwera
    fd = bind_inet_socket(port, SOCK_DGRAM);
    doServer(fd);

    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");

    // Sprzątanie
    pthread_mutex_destroy(&stack.mutex);
    sem_destroy(&stack.sem_filled);

    return EXIT_SUCCESS;
}