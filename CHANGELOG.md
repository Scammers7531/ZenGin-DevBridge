[🇵🇱 Polski](CHANGELOG.md) | [🇬🇧 English](CHANGELOG.en.md)

# Dziennik zmian

## 0.2.0 — kandydat lokalny, 2026-11-27 (nieopublikowany)

- Rozszerzono API o limitowane wyszukiwanie i hierarchię VObów, diagnostykę NPC oraz szczegółowy stan bohatera i jego kolizji.
- Dodano inspekcję waypointów, lokalnego grafu WayNetu, wykrywanie odizolowanych i niespójnych połączeń oraz trasę wyznaczaną ograniczonym algorytmem Dijkstry. Wynik grafowy jawnie nie potwierdza fizycznej przechodniości.
- Dodano wyszukiwanie i typowany, ograniczony odczyt symboli Daedalusa; plugin nie zapisuje dowolnych zmiennych.
- Dodano odczyt stanu zCMovera i ograniczone monitorowanie bohatera, NPC lub movera z limitem czasu, próbek, anulowaniem i obsługą braku postępu.
- Dodano screenshot bieżącej klatki renderera do kontrolowanego katalogu wymiany w formacie TGA.
- Dodano opcjonalne komendy player_teleport, player_turn, player_move i player_walk_to. Są domyślnie wyłączone i zawsze przywracają poprzedni stan wejścia po zakończeniu operacji.
- Komendy ruchu podają wejście przed przetworzeniem AI gracza i raportują liczbę klatek AI (`control_ai_frames`), aby klient mógł odróżnić obsługę żądania od rzeczywistego ruchu.
- Dodano skrypt regresyjny sterowania postacią, który wykrywa wolny kierunek i sprawdza rzeczywisty ruch oraz dojście do celu w sterowalnym świecie.
- Zachowano protokół 1 i istniejące komendy v0.1.1; klient PowerShell akceptuje nowe nazwy komend.
- Końcowy przegląd źródeł wykrył, że skonfigurowane int będące zmienną klasową nie mają kontekstu instancji wymaganego przez GetValue/SetValue. Weryfikacja scenario_start i scenario_status odrzuca teraz takie symbole jako configured_symbol_type_mismatch; kolejka nadal musi być zapisywalna.
- Poprawiono normalizację poziomego kierunku przed SetHeadingAtWorld, aby wektor celu nie zniekształcał orientacji bohatera.
- Finalna DLL Windows x86: SHA-256 B28E10A4441DA8DE7687C60AF2E99B27B1B7F3B0A8F21E063A737032216D4CC4. Na tym pliku zaliczono 11/11 testów runtime i 7/7 testów zgodności wstecznej.
- Na finalnej DLL odczytano globalne symbole float, string i tablice, bezpiecznie odrzucono classvar jako symbol kolejki i statusu, a poprawny globalny int zadziałał jako kolejka i status. player_turn zmienił kierunek na [1,0,0] i przywrócił wcześniejszy; player_teleport przesunął bohatera o 100 jednostek X i wrócił do pozycji początkowej.
- mover_inspect zwrócił stan istniejącego zCMover, a monitor próbkował Xardasa i mover w czterech próbkach. screenshot zapisał poprawny obraz TGA 800×600×32; plik został zweryfikowany wizualnie i usunięty z testbedu.
- vob_hierarchy zwrócił prawidłowe dane celu, ale bez przodków i dzieci w testowanym świecie; nie potwierdzono niepustej relacji drzewa.
- Hook ruchu na finalnej DLL otrzymał wejście przez 74 klatki AI w scenie dialogowej, lecz distance_moved wyniósł tylko 0,023 jednostki; wejście zostało zwolnione. Poprzedni build poruszył postać w swobodnej rozgrywce, ale ta trasa nie została powtórzona na finalnym SHA. player_walk_to przeszło test no-op i cleanup, lecz rzeczywiste dojście do celu na finalnej DLL pozostaje niezweryfikowane.
- Przy domyślnej konfiguracji ping zgłosił sterowanie wyłączone, a player_move otrzymało capability_disabled.
- Test anulowania długiego monitorowania zwrócił timeout, usunął aktywne żądanie i pozostawił działający ping. Pomiar wydajności finalnej DLL w 10-sekundowej scenie intro: bezczynnie 10,328 s CPU / 10,09 s czasu ściennego (102,39% jednego rdzenia), 393,5 MiB RAM; podczas skanów VObów 7,516 s CPU / 10,19 s (73,73%), średnio 15 706 węzłów, 374 ms na żądanie i szczyt 1 548 ms, RAM 393,5→393,7 MiB. Obciążenie innych procesów i koszt uruchamiania PowerShell uniemożliwiają wyciągnięcie z tego pomiaru izolowanego narzutu pluginu.
- Kandydat pozostaje lokalny. Rzeczywiste przejście trasy na finalnej DLL nie zostało potwierdzone, dlatego v0.2.0 nie jest jeszcze gotowe do publicznego wydania.
- Wydanie pozostaje lokalnym kandydatem; brak tagu, publikacji oraz wdrożenia do GO3 lub GO3 DEV.

## 0.1.1 — 2026-11-26 (wydanie przedpremierowe, pakiet zweryfikowany)

- Rozdzielono wymagania uruchamiania gotowej DLL i kompilacji ze źródeł; dodano bezpośredni odnośnik do wydania oraz praktyczne zastosowania pluginu.
- Uzupełniono dokumentację protokołu o wszystkie kody błędów, odrzucenia bez `response.json`, terminy, anulowanie, przywracanie kamery, ponawianie scenariuszy i wczytywanie konfiguracji.
- Zabezpieczono `client.lock`, aby klient, który nie uzyskał blokady, nie usuwał pliku należącego do innego procesu.
- Dodano sprawdzanie, czy skonfigurowane symbole Daedalusa są zmiennymi całkowitoliczbowymi o jednym elemencie, a symbol kolejki nie jest stałą.
- Dodano regresyjny test współbieżnych klientów; tag i wydanie `v0.1.0` pozostają bez zmian.
- Zweryfikowano DLL w działającym Gothic II 2.6 (fix) z Union 1.0M: `ping`, `vob`, `raycast`, `surface_grid`, `camera_get` i bezpieczne `camera_restore`; protokół pozostaje w wersji 1.
- Testy parsera Daedalusa potwierdziły odczyt skalarnego `int`, poprawny zapis i ponowny odczyt testowej kolejki oraz odrzucenie stałej i symbolu instancji przez właściwe kody błędów.
- SHA-256 lokalnej kompilacji kandydackiej Windows x86: `C4E8213AE394352550FF31709A83E6FEA8FC6118CCDA7435554BE47AB552644A`.

## Aktualizacja dokumentacji — 2026-11-26

- Ustawiono polski jako domyślny język dokumentacji publicznej i dodano pełne wersje angielskie z przełącznikami języka.
- Dodano polską i angielską dokumentację API oraz odizolowanego środowiska testowego.

## 0.1.0 Public Preview — 2026-11-26

- Pierwsze samodzielne publiczne wydanie preview dla Gothic II: Noc Kruka (x86).
- Zachowano inspekcję VObów, pojedyncze i seryjne raycasty, siatkę powierzchni oraz odczyt i sterowanie kamerą.
- Dodano wersję protokołu 1, raportowanie capabilities, ustrukturyzowane kody błędów, terminy żądań, znaczniki anulowania i dzienniki diagnostyczne.
- Zmiany kamery i scenariuszy wymagają jawnego włączenia w lokalnym pliku konfiguracji katalogu głównego gry.
- Dodano kontrolę zmiany świata i bezpieczne przywracanie kamery, gdy świat lub kamera są niedostępne albo zostały zastąpione.
- Usunięto wbudowane, zależne od konkretnego środowiska nazwy scenariuszy i symbole.
- Dodano niezależny system kompilacji CMake, instrukcję instalacji, dokumentację API i informacje o licencjach stron trzecich.

### Znane ograniczenia wydania preview

- Obsługiwana i kompilowana jest wyłącznie wersja Gothic II: Noc Kruka.
- Podstawowe działanie sprawdzono z Gothic II 2.6 (fix) i Union 1.0M w odizolowanym środowisku testowym; inne konfiguracje wymagają osobnej weryfikacji.
- Krótki pomiar bezczynności nie wykazał mierzalnego narzutu w testowanej scenie; nie jest to długotrwały ani międzyplatformowy benchmark.
- Wysyłanie scenariuszy wymaga lokalnie skonfigurowanego symbolu całkowitoliczbowego Daedalusa i pozostaje domyślnie wyłączone.
- Sterowanie kamerą wymaga jawnego lokalnego włączenia; zapisane ustawienia można przywrócić tylko wtedy, gdy pierwotny świat i kamera są nadal dostępne.
