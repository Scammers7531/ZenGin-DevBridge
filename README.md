[🇵🇱 Polski](README.md) | [🇬🇧 English](README.en.md)

# ZenGin DevBridge

ZenGin DevBridge to lokalny plugin diagnostyczny oparty na wymianie plików dla **Gothic II: Noc Kruka (x86)**. Umożliwia lokalnym skryptom i agentom AI odczytywanie stanu załadowanego świata przez Union API, bez uruchamiania serwera sieciowego.

Wersja `0.1.1` jest wydaniem przedpremierowym zweryfikowanym w izolowanym testbedzie Gothic II: Noc Kruka 2.6 (fix) z Union 1.0M. Możesz użyć gotowej biblioteki DLL albo skompilować projekt ze źródeł; wymagania tych dwóch sposobów są opisane osobno poniżej. Plugin działa wewnątrz aktywnego procesu gry.

**Gotowe wydanie przedpremierowe:** [Pobierz paczkę ZenGinDevBridge-0.1.1-win32.zip](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.1.1/ZenGinDevBridge-0.1.1-win32.zip) · [opis wydania i sumy SHA-256](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.1.1). Do użycia DLL nie są potrzebne narzędzia programistyczne.

## Dostępne narzędzia

- Odczyt klasy VOba, transformacji, granic przestrzennych i flag kolizji.
- Wykonywanie pojedynczego raycastu lub ograniczonej liczby raycastów w jednej serii.
- Próbkowanie ograniczonej siatki X/Z `surface_grid`, z porównaniem trafień w świat statyczny, dynamiczne VOby i bounding boxy oraz odczytami podłoża, wody i sufitu dla gracza.
- Odczyt transformacji kamery oraz jej flag AI.
- Opcjonalne ustawianie i przywracanie kamery. Zmiany kamery są domyślnie wyłączone.
- Opcjonalne wysyłanie żądań i odczyt skonfigurowanych symboli całkowitoliczbowych Daedalusa. Obsługa scenariuszy jest domyślnie wyłączona i nie zawiera wbudowanych nazw scen.
- Odczyt wersji protokołu i możliwości komend za pomocą `ping`.

Domyślnie dostępne są komendy tylko do odczytu. Operacje camera_set, scenario_start, player_teleport, player_turn, player_move i player_walk_to zmieniają stan i wymagają lokalnego włączenia oraz sprawdzenia capabilities przez ping. camera_restore jest zawsze dostępne, aby usunąć nadpisanie kamery ustawione przez DevBridge.

## Praktyczne zastosowania

- Sprawdź, czy `PC_HERO` lub inny VOb istnieje w załadowanym świecie, oraz odczytaj jego klasę, transformację i granice.
- Porównaj geometrię wizualną, świat statyczny i dynamiczne VOby, aby znaleźć rozbieżności kolizji za pomocą `raycast`, `raycast_batch` i `surface_grid`.
- Zapisz wyniki do JSON lub CSV i porównuj je między zmianami mapy albo konfiguracji moda.
- Ustaw kamerę do powtarzalnego przeglądu sceny, jeśli lokalnie włączono sterowanie; po inspekcji przywróć pierwotne ustawienie przez `camera_restore`.
- W testowej instalacji wyślij lokalnie skonfigurowany scenariusz i odczytaj skonfigurowane liczniki/statusy bez dodawania nazw scenariuszy do publicznego pluginu.

## Lokalny kandydat v0.2.0

Najnowszym publicznym wydaniem pozostaje v0.1.1. Rozszerzenia opisane poniżej są przygotowanym lokalnie kandydatem v0.2.0; nie są opublikowane i nie mają jeszcze publicznej paczki do pobrania.

Kandydat dodaje:

- limitowane wyszukiwanie VObów, hierarchię oraz wyszukiwanie i inspekcję NPC;
- szczegółowy stan bohatera i odczyty kolizji, podłoża, spadania oraz przeszkód;
- inspekcję waypointów, grafu WayNetu, jego spójności i tras obliczanych algorytmem Dijkstry;
- wyszukiwanie metadanych i typowany odczyt wartości Daedalusa: int, float, string oraz ograniczonych zakresów tablic;
- inspekcję stanu i klatek kluczowych zCMover; komenda nie uruchamia movera;
- ograniczony czasowo monitor pozycji bohatera, NPC lub movera, z limitem próbek i wykrywaniem braku postępu;
- screenshot bieżącej klatki renderera do pliku TGA w katalogu wymiany;
- opcjonalne, domyślnie wyłączone polecenia player_teleport, player_turn, player_move i player_walk_to.

Sterowanie postacią wymaga jawnego allow_player_control:true w lokalnej konfiguracji. Teleport nie jest ruchem. player_walk_to ustawia kierunek i emuluje wejście do przodu; nie wyznacza ścieżki ani nie omija przeszkód. Na finalnej DLL B28 wczytanej w zewnętrznym zapisie player_move forward przesunęło bohatera o 178,11 jednostki w 713 ms; player_walk_to osiągnęło dwa cele, a podejście do stromego stoku zakończyło się movement_stalled po 188,71 jednostki i zwolniło wejście.

W otwartym, podpartym miejscu testbedu sprawdzono oba kierunki boczne pluginu przez około sekundę. Każde wejście dotarło do 83–87 klatek AI i zostało zwolnione, ale pozycje X/Z nie zmieniły się (distance_moved wyniósł 0,092–0,108 jednostki wskutek ruchu pionowego). Następnie, w tym samym profilu i na tym samym punkcie, ręczne przytrzymanie zwykłych klawiszy Strafe Left i Strafe Right przesunęło bohatera odpowiednio o około +351 i −659 jednostek osi X. To potwierdza, że w tym stanie świata sterowanie boczne silnika działa, a rozbieżność dotyczy ścieżki wejścia pluginu. Traktuj strafe_left i strafe_right w player_move jako eksperymentalne i sprawdzaj pozycje X/Z oraz distance_moved; control_ai_frames i reason:duration_elapsed nie dowodzą ruchu.

Finalna lokalna DLL kandydata ma SHA-256 B28E10A4441DA8DE7687C60AF2E99B27B1B7F3B0A8F21E063A737032216D4CC4. Na tym pliku zaliczono 11/11 testów runtime i 7/7 testów zgodności wstecznej, a dodatkowo sprawdzono zmianę orientacji, symbole Daedalusa, monitor NPC/movera, obraz TGA i ruch w Gothicu. W testowanym świecie vob_hierarchy zwrócił poprawny cel, lecz puste listy przodków i dzieci. Na finalnej DLL potwierdzono ruch do przodu, osiągnięcie celów przez player_walk_to oraz wykrycie zatrzymania na stromym stoku. Ręczne sterowanie klawiaturą przesuwało bohatera w bok w tej samej konfiguracji, podczas gdy strafe pluginu nie zmieniał X/Z; ruch boczny przez player_move pozostaje więc ograniczeniem eksperymentalnym.

Wcześniejszy odczyt player_state wskazał pozycję oddaloną o około 74,646 jednostki od końca poprzedniego player_move forward. Oba odczyty korzystają z tej samej pozycji świata VOba i standardowej odległości euklidesowej; przesunięcie jest niemal dokładnie zgodne z poprzednim kierunkiem chodu i odpowiada około 0,30 s ruchu z wcześniejszą prędkością. To pasuje do późniejszego odczytu stanu gry po upływie czasu, a nie wskazuje na błąd obliczenia pozycji. Nie zapisano dokładnego odstępu czasu ani stanu wejścia, więc źródła tej dodatkowej zmiany nie da się rozstrzygnąć. Ograniczenia te są udokumentowane; kandydat jest gotowy do rozważenia jako Pre-release, pozostaje lokalny i nie został opublikowany.

### Przykłady dla Codexa, Claude Code i własnych skryptów

Codex, Claude Code oraz własny skrypt mogą wywołać tego samego klienta PowerShell i analizować jego JSON. Poniższe zapytania są tylko do odczytu:

    $client = '.\client\powershell\Query-DevBridge.ps1'
    & $client -GameRoot 'C:\Games\Gothic II' -Command vob_search -PayloadJson '{"class_contains":"zCMover","limit":20,"scan_limit":20000}'
    & $client -GameRoot 'C:\Games\Gothic II' -Command waypoint_search -PayloadJson '{"limit":64,"offset":0}'
    & $client -GameRoot 'C:\Games\Gothic II' -Command daedalus_read -PayloadJson '{"symbol":"TRUE","count":1}'

W poleceniu dla Codexa lub Claude Code poproś o sprawdzenie capabilities przez ping, odczytanie wyników JSON i wyraźne oddzielenie danych niedostępnych od wartości zerowych. Niech agent zaznaczy, że trasa WayNetu i raycasty nie potwierdzają możliwości przejścia postaci. Polecenia zmieniające stan wymagają osobnej, jawnej zgody oraz lokalnego włączenia w konfiguracji.

### Eksport wyników do JSON i CSV

Dołączony klient zapisuje pełną odpowiedź JSON przez parametr -SaveJsonPath. Parametr -ExportCsvPath tworzy spłaszczone wiersze CSV dla komend raycast_batch i surface_grid:

    $client = '.\client\powershell\Query-DevBridge.ps1'
    $gameRoot = 'C:\Games\Gothic II'
    $payload = @{ default_flags = 1058; probes = @(@{ start = @(100, 1000, 200); direction = @(0, -1500, 0) }) } | ConvertTo-Json -Depth 4 -Compress
    & $client -GameRoot $gameRoot -Command raycast_batch -PayloadJson $payload -SaveJsonPath '.\raycasts.json' -ExportCsvPath '.\raycasts.csv'

## Wymagania

### Korzystanie z gotowej biblioteki DLL

- Windows 10 lub nowszy.
- Gothic II: Noc Kruka w wersji x86 z kompatybilną instalacją Union.
- Windows PowerShell 5.1 lub PowerShell 7, jeśli korzystasz z dołączonego klienta; skrypty testowe wymagają PowerShell 7 (pwsh).
- Pobierz [gotową paczkę ZIP](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.1.1/ZenGinDevBridge-0.1.1-win32.zip) lub sprawdź [opis wydania](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.1.1).

Do uruchomienia gotowej DLL nie są potrzebne Visual Studio, CMake, Ninja ani Git. Gothic II i Union muszą być już zainstalowane.

### Kompilacja ze źródeł

- Windows z Visual Studio 2022 C++ Build Tools dla x86 oraz Windows 10 SDK.
- CMake 3.24 lub nowszy, Ninja i Git.

Gra i kompatybilna instalacja Union są potrzebne do uruchomienia oraz testów runtime, ale nie do samej kompilacji. Projekt kompiluje się z przypiętymi podmodułami Union API i Gothic API; repozytorium nie zawiera prywatnych materiałów projektowych ani zasobów gry.

## Kompilacja

Sklonuj repozytorium wraz ze wszystkimi zależnościami źródłowymi:

```powershell
git clone --recurse-submodules https://github.com/Scammers7531/ZenGin-DevBridge.git
cd ZenGin-DevBridge
```

Skrypt kompilacji przyjmuje ścieżki do lokalnego toolchainu kompilatora, CMake, Ninja i Windows SDK. W typowej instalacji Visual Studio SDK znajduje się poza katalogiem kompilatora:

```powershell
.\scripts\build.ps1 `
  -ToolchainRoot 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' `
  -WindowsSdkRoot 'C:\Program Files (x86)\Windows Kits\10' `
  -CMakePath 'C:\Program Files\CMake\bin\cmake.exe' `
  -NinjaPath 'C:\Program Files\CMake\bin\ninja.exe'
```

Wynikowa biblioteka DLL znajduje się w `build\g2a-release\ZenGinDevBridge.dll`. Skrypt jej nie instaluje ani nie wdraża do gry.

## Instalacja

1. Wykonaj kopię zapasową obecnej konfiguracji Union w docelowej instalacji gry i zamknij grę.
2. Skopiuj `ZenGinDevBridge.dll` do `<game-root>\System\Autorun\`.
3. Uruchom Gothic II: Noc Kruka. Gdy wykona się hook renderujący, plugin utworzy katalog `<game-root>\ZenGinDevBridge\`.
4. Uruchom klienta PowerShell z tego repozytorium, podając ścieżkę do gry:

   ```powershell
   .\client\powershell\Query-DevBridge.ps1 -GameRoot 'C:\Games\Gothic II' -Command ping
   ```

   Możesz też użyć parametru `-ExchangeDirectory`, aby wskazać istniejący katalog wymiany.

Plugin i klient wymieniają lokalnie pliki JSON w kodowaniu UTF-8. Plugin nie uruchamia serwera ani nie wysyła telemetrii. Katalog wymiany musi być zapisywalny przez proces gry.

## Konfiguracja lokalna

Umieść plik `ZenGinDevBridge.config.json` w katalogu głównym gry, obok folderu `System`. Narzędzia tylko do odczytu nie wymagają konfiguracji.

```json
{
  "config_version": 1,
  "exchange_directory": "ZenGinDevBridge",
  "allow_camera_control": false,
  "allow_scenario_start": false,
  "allow_player_control": false
}
```

Ustaw `allow_camera_control` na `true` tylko wtedy, gdy chcesz zezwolić na `camera_set`. Przywracanie kamery jest zawsze dozwolone. Wysyłanie scenariusza wymaga także lokalnego mapowania `scenario_dispatch`; zobacz [opis API i konfiguracji](docs/API.md#konfiguracja-lokalna). Pozostaw ten plik lokalnie i przed włączeniem sprawdź każde mapowanie symbolu.

Plugin wczytuje konfigurację przy pierwszym uruchomieniu hooka renderującego po starcie gry i zachowuje ją w pamięci do końca procesu. Po każdej zmianie pliku zamknij i uruchom ponownie grę. Samo ponowne uruchomienie klienta PowerShell nie przeładuje konfiguracji pluginu.

## Przykłady dla agentów AI

Użyj dołączonego klienta do wykonania pojedynczego zapytania tylko do odczytu:

```powershell
$client = '.\client\powershell\Query-DevBridge.ps1'
& $client -GameRoot 'C:\Games\Gothic II' -Command ping
& $client -GameRoot 'C:\Games\Gothic II' -Command vob -Name PC_HERO
& $client -GameRoot 'C:\Games\Gothic II' -Command raycast `
  -Start 100,1000,200 -Direction 0,-1500,0 -Flags 1058
```

Agent komunikujący się bezpośrednio przez protokół plikowy powinien wysyłać tylko jedno żądanie naraz, podawać wersję protokołu 1 i termin ważności, oczekiwać na `response.json` z pasującym `id` oraz traktować `ok: false` jako błąd. Poniższy JSON pokazuje format żądania. Dla każdego żądania oblicz nową wartość `expires_at_unix_ms`; nie kopiuj przykładowej daty:

```json
{
  "id": "agent-0001",
  "command": "surface_grid",
  "protocol_version": 1,
  "expires_at_unix_ms": 1795700000000,
  "center": [100, 1000, 200],
  "radius_x": 20,
  "radius_z": 20,
  "spacing": 20,
  "down_range": 2000,
  "include_bbox": false
}
```

Pełny format żądań, pola odpowiedzi, błędy i zabezpieczenia operacji zmieniających stan opisuje [docs/API.md](docs/API.md).

### Codex i Claude Code

Codex, Claude Code oraz inni agenci programistyczni mogą uruchamiać dołączonego klienta PowerShell w swoim terminalu i analizować zwrócony JSON. Przykład polecenia dla agenta, które nie zmienia stanu gry:

```text
Użyj klienta PowerShell ZenGin DevBridge. Najpierw wywołaj ping i sprawdź world_ready oraz capabilities. Następnie sprawdź PC_HERO komendą vob i zbadaj podłoże za pomocą surface_grid. Opisz wyniki JSON. Nie wywołuj camera_set, scenario_start ani player_*.
```

Jeśli jawnie zezwolisz na zmianę kamery, poproś agenta o sprawdzenie, czy `camera_set` jest dostępne, wykonanie żądanej operacji i późniejsze wywołanie `camera_restore`. Wysłanie scenariusza wymaga osobnej, jawnej zgody oraz sprawdzonego lokalnego mapowania symbolu. Przewodnik API opisuje ten sam sposób pracy dla klientów wysyłających pliki JSON bezpośrednio.

## Kompatybilność i wsparcie

- To wydanie preview obsługuje wyłącznie Gothic II: Noc Kruka.
- Testy w odizolowanym środowisku wykonano z Gothic II 2.6 (fix) i Union 1.0M.
- Dla zachowania zgodności starsze klienty mogą pomijać `protocol_version`; takie żądania są traktowane jako wersja 1.
- Nazwy scenariuszy i symbole Daedalusa zależne od środowiska należą do lokalnej konfiguracji, a nie do publicznego kodu pluginu.
- Przy zweryfikowanej kompilacji lub późniejszym wdrożeniu zapisuj rewizję źródeł, wersję pluginu i SHA-256 DLL.

## Podstawowe testy protokołu

Dołączone testy sprawdzają wymianę protokołu, anulowanie po przekroczeniu terminu oraz współbieżne uruchomienie dwóch klientów współdzielących katalog wymiany. Test blokady potwierdza, że klient, który jej nie uzyskał, nie usuwa blokady ani żądania właściciela:

```powershell
pwsh -NoProfile -File .\tests\Invoke-MockProtocolTests.ps1
pwsh -NoProfile -File .\tests\Invoke-ClientLockTests.ps1
```

Testy weryfikują plikowy przepływ klienta. Podsumowanie kontroli runtime znajduje się w dzienniku zmian; wyniki mogą się różnić w zależności od instalacji Gothica, wersji Union i moda.

Test sterowania postacią uruchamiaj wyłącznie w izolowanym testbedzie z włączonym allow_player_control:true, po zakończeniu sceny fabularnej i na podłożu bez przeszkód w pobliżu. Zastąp `<testbed-path>` rzeczywistą ścieżką katalogu głównego testbedu. Skrypt sam wybiera kierunek z wolnym krótkim raycastem, sprawdza rzeczywisty ruch oraz dojście do celu:

```powershell
pwsh -NoProfile -File .\tests\Invoke-PlayerControlTests.ps1 -GameRoot '<testbed-path>'
```

## Licencja

Kod źródłowy DevBridge jest udostępniany na licencji MIT. Informacje o kodzie stron trzecich i wymagane treści licencji znajdują się w [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Projekt nie rozpowszechnia szablonu pluginu Union używanego na wczesnym etapie prac; repozytorium tego szablonu nie publikuje licencji, dlatego jego pliki bazowe i pozostałe materiały zostały wykluczone.
