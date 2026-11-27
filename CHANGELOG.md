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
- W działającym Gothic II 2.6 (fix) z Union 1.0M sprawdzono komunikację, zgodność dotychczasowych komend, nowe diagnostyki, typy poprawnych i niepoprawnych symboli Daedalusa, anulowanie monitora, screenshot oraz rzeczywisty player_move i player_walk_to. Ruch zmienił pozycję postaci, osiągnął cel w zadanej tolerancji i zwolnił wejście.
- Test anulowania długiego monitorowania zwrócił timeout, usunął aktywne żądanie i pozostawił działający ping. Wynik screenshotu w formacie TGA został sprawdzony i usunięty z katalogu wymiany.
- Kandydacka biblioteka Windows x86: SHA-256 89B34B07E6A8F3813B7284D71F8B90FCA300DCA18AB9D0EE230CF8A5311D43F5.
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
