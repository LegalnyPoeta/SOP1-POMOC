void doServer(int fd) {
    struct sockaddr_in addr;
    char buf[512]; // Zwykły bufor na tekst

    while (1) {
        socklen_t size = sizeof(addr);
        int receivedBytes;

        // Odbieramy datagram
        if ((receivedBytes = TEMP_FAILURE_RETRY(recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&addr, &size))) < 0)
            ERR("read:");
            
        buf[receivedBytes] = 0; // Upewniamy się, że to poprawny string w C (dodajemy null-terminator)

        // Zmienne na dane z zadania
        int x, y, p;
        char name[129]; // 128 znaków + \0

        // Parsowanie wiadomości tekstowej od netcata
        // Format: <X> <Y> <P> <nazwa oddziału>
        if (sscanf(buf, "%d %d %d %128[^\n]", &x, &y, &p, name) == 4) {
            
            // Prosta walidacja na podstawie treści zadania
            if (x >= 0 && x <= 99 && y >= 0 && y <= 99 && (p == 0 || p == 1)) {
                // Poprawna wiadomość
                printf("%s oddział %s był widziany na pozycji %d:%d.\n", 
                       (p == 1) ? "Nasz" : "Wrogi", 
                       name, x, y);
            } else {
                fprintf(stderr, "Błąd: Nieprawidłowe wartości (X: %d, Y: %d, P: %d)\n", x, y, p);
            }
            
        } else {
            // Złe formatowanie (brakujące argumenty, itp.)
            fprintf(stderr, "Błąd parsowania: Zły format wiadomości -> %s\n", buf);
        }
    }
}