#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

// Makro ponawiające próbę w przypadku przerwania przez sygnał (EINTR)
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

// Makro do rzucania błędem i zamykania programu
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// GWARANTOWANE ODCZYTYWANIE CAŁOŚCI (Dla protokołów strumieniowych)
ssize_t bulk_read(int fd, char *buf, size_t count) {
    int c;
    size_t len = 0;
    do {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0) return c; // Blad
        if (0 == c) return len; // Zamknieto polaczenie (EOF)
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

// GWARANTOWANE ZAPISYWANIE CAŁOŚCI
ssize_t bulk_write(int fd, char *buf, size_t count) {
    int c;
    size_t len = 0;
    do {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0) return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

// Ustawianie trybu nieblokujacego na gniezdzie
void set_nonblock(int sock) {
    int flags = fcntl(sock, F_GETFL);
    if (flags == -1) ERR("fcntl get");
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) ERR("fcntl set");
}

#endif