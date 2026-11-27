[🇵🇱 Polski](CHANGELOG.md) | [🇬🇧 English](CHANGELOG.en.md)

# Dziennik zmian

## 0.2.0 — 2026-11-27 (wydanie przedpremierowe)

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
- Hook ruchu na finalnej DLL otrzymał wejście przez 74 klatki AI w scenie dialogowej, lecz distance_moved wyniósł 0,023 jednostki; wejście zostało zwolnione. Późniejszy test na wczytanym zewnętrznym zapisie potwierdził player_move forward (178,11 jednostki / 713 ms), dojście player_walk_to do dwóch celów oraz movement_stalled na stromym stoku (188,71 jednostki ruchu, 96,12 jednostki do celu, input_released=true). Na otwartym podłożu testy strafe pluginu przez 87 i 83 klatki AI nie zmieniły X/Z (distance_moved 0,092 i 0,108, wyłącznie zmiana Y); wejście zostało zwolnione. Następnie ręczne przytrzymanie zwykłych klawiszy Strafe Left i Strafe Right w tym samym profilu i miejscu przesunęło bohatera odpowiednio o około +351 i −659 jednostek osi X. Ruch silnika działał, a ograniczenie dotyczy ścieżki wejścia pluginu; player_move strafe pozostaje eksperymentalne.
- Rozbieżność około 74,646 jednostki pomiędzy końcową pozycją jednej odpowiedzi ruchu i późniejszym player_state sprawdzono w kodzie: obie komendy odczytują tę samą pozycję świata VOba i stosują normę euklidesową. Dodatkowy wektor jest niemal dokładnie równoległy do poprzedniego ruchu i odpowiada około 0,30 s chodu przy zmierzonej prędkości. Jest zgodny z odczytem późniejszego stanu świata; dokładny odstęp czasu i stan wejścia nie zostały zapisane, więc konkretnej przyczyny nie da się potwierdzić. Nie znaleziono dowodu na błąd pomiaru.
- Przy domyślnej konfiguracji ping zgłosił sterowanie wyłączone, a player_move otrzymało capability_disabled.
- Test anulowania długiego monitorowania zwrócił timeout, usunął aktywne żądanie i pozostawił działający ping. Jedyny pomiar wydajności z pełnymi danymi w lokalnym dzienniku wykonano na porównawczej DLL SHA-256 `89B34B07E6A8F3813B7284D71F8B90FCA300DCA18AB9D0EE230CF8A5311D43F5`, a nie na finalnej DLL B28: w 15-sekundowym pomiarze bezczynności Gothica zużycie CPU wyniosło 15,406 s / 15,025 s czasu ściennego (102,54% jednego rdzenia), a pamięć 392,5→393,5 MiB; bez pluginu 15,031 s / 15,039 s (99,95%), pamięć 388,1→386,8 MiB. Podczas ośmiu skanów VObów zanotowano 13,347 s czasu łącznego, średnio 1 662 ms na żądanie, 97,51% jednego rdzenia i średnio 16 816 węzłów; czas żądania obejmował uruchomienie osobnego klienta PowerShell. Wyniki nie izolują narzutu pluginu. Nie ma porównywalnego pomiaru wydajności dla DLL B28, więc liczb z innych prób nie przypisujemy finalnemu plikowi.
- Finalna DLL B28 potwierdziła ruch do przodu, dojście do celów i wykrywanie zatrzymania; zwykłe klawisze pozwalały chodzić w bok, choć strafe pluginu nie zmieniał X/Z. Ograniczenie ruchu bocznego pozostaje eksperymentalne; przyczyny dodatkowego przesunięcia między odczytami pozycji nie potwierdzono.
- Opublikowano [v0.2.0 jako Pre-release](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.2.0). Tag źródłowy pozostał przypięty do historycznego commita; późniejsze dokumenty z gałęzi `main` i paczka dystrybucyjna są aktualizowane bez przepisywania tagu. Aktualną sumę ZIP publikuje plik `SHA256SUMS-v0.2.0.txt`. DLL zachowuje SHA-256 `B28E10A4441DA8DE7687C60AF2E99B27B1B7F3B0A8F21E063A737032216D4CC4`.

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
