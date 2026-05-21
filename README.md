Oto jak krok po kroku przetestować Twój serwer za pomocą narzędzia `netcat` (często w systemach ukrytego pod poleceniem `nc`). Będziesz potrzebował **dwóch otwartych okien terminala**.

### Krok 1: Uruchomienie serwera (Terminal 1)

Skompiluj swój kod i uruchom serwer, podając mu numer portu (np. `8080`):

```bash
gcc server.c -o server
./server 8080

```

*Serwer teraz "wisi" i nasłuchuje.*

### Krok 2: Uruchomienie klienta netcat (Terminal 2)

W drugim oknie połącz się z serwerem. Flaga `-u` oznacza, że wymuszasz użycie protokołu **UDP** (domyślnie netcat używa TCP). Używamy `127.0.0.1` (localhost), bo testujesz to na tym samym komputerze.

```bash
nc -u 127.0.0.1 8080

```

*Po wciśnięciu Enter nie zobaczysz żadnego potwierdzenia – to normalne w UDP. Program czeka, aż wpiszesz tekst.*

### Krok 3: Wpisywanie komend testowych

Teraz w Terminalu 2 (tam gdzie działa netcat) wpisuj wiadomości i po każdej wciskaj **Enter**. Obserwuj, co wypisuje Terminal 1 (serwer).

**Test 1: Poprawny oddział sojuszniczy (P = 1)**

```text
15 42 1 Gwardia Cesarska

```

*W Terminalu 1 powinieneś zobaczyć:* `Nasz oddział Gwardia Cesarska był widziany na pozycji 15:42.`

**Test 2: Poprawny wrogi oddział (P = 0)**

```text
99 5 0 Prusacy Wellingtona

```

*W Terminalu 1 powinieneś zobaczyć:* `Wrogi oddział Prusacy Wellingtona był widziany na pozycji 99:5.`

**Test 3: Błędny format (brakuje P)**

```text
10 20 Zly format danych

```

*W Terminalu 1 powinieneś zobaczyć:* `Błąd parsowania: Zły format wiadomości...` (serwer nie powinien się wywalić, tylko czekać na kolejne).

**Test 4: Współrzędne poza zakresem (>99)**

```text
150 20 1 Szaserzy

```

*W Terminalu 1 powinieneś zobaczyć:* `Błąd: Nieprawidłowe wartości (X: 150, Y: 20, P: 1)`

---

### Szybki trik dla leniwych (Wysyłanie "One-linerem")

Jeśli nie chcesz otwierać interaktywnej sesji netcata, możesz wysłać pojedynczy pakiet z terminala używając `echo` i potoku (`|`). To bardzo wygodne do testów:

```bash
echo "45 50 1 Szwolezerowie" | nc -w 1 -u 127.0.0.1 8080

```

*(Flaga `-w 1` każe netcatowi zamknąć się po 1 sekundzie od wysłania danych, żeby nie wisiał w nieskończoność).*