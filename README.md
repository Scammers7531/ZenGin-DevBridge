[🇵🇱 Polski](README.md) | [🇬🇧 English](README.en.md)

# ZenGin DevBridge

Lokalny plugin diagnostyczny i automatyzacyjny dla **Gothic II: Noc Kruka (x86)**. Udostępnia dane działającego silnika ZenGin przez Union API, pozwalając modderom, skryptom i agentom AI obserwować świat gry oraz wykonywać kontrolowane testy rozgrywki. Komunikacja odbywa się przez lokalne pliki JSON; plugin nie otwiera portu sieciowego ani nie wysyła telemetrii.

**Aktualne wydanie: v0.2.0 (Pre-release) · 31 komend API · protokół 1 · licencja MIT**

[Pobierz ZIP](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.2.0/ZenGinDevBridge-0.2.0-win32.zip) · [Informacje o wydaniu](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.2.0) · [SHA-256 ZIP](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.2.0/SHA256SUMS-v0.2.0.txt)

## Możliwości

| Obszar | Co umożliwia |
| --- | --- |
| Obiekty świata | Inspekcja, wyszukiwanie i dostępne relacje hierarchii VObów. |
| Geometria i kolizje | Raycasty pojedyncze i grupowe, siatki powierzchni, sondy podłoża oraz przeszkód bohatera. |
| NPC i bohater | Odczyt dostępnych danych o pozycji, zdrowiu, AI, rutynie, animacji, ekwipunku i stanie ruchu. |
| WayNet | Wyszukiwanie waypointów, analiza grafu i obliczanie tras grafowych. |
| Daedalus | Wyszukiwanie symboli, typowany odczyt int, float, string i zakresów tablic; opcjonalna wysyłka skonfigurowanych scenariuszy. |
| Ruchome obiekty | Odczyt stanu, parametrów i klatek kluczowych zCMovera. |
| Kamera i obraz | Odczyt, opcjonalne ustawianie i przywracanie kamery oraz zapis klatki TGA. |
| Sterowanie bohaterem | Opcjonalny teleport, obrót, rzeczywista próba ruchu i dojścia do celu. |
| Monitoring | Ograniczone czasowo próbkowanie stanu bohatera, NPC lub movera. |

Przykładowe zastosowania: diagnozowanie kolizji schodów i platform, sprawdzanie stanu NPC i zmiennych fabularnych, kontrolowanie działania WayNetu, porównywanie pozycji obiektów i wykonywanie powtarzalnych prób ruchu w instalacji testowej.

## Wymagania

- Windows 10 lub nowszy; Gothic II: Noc Kruka x86 z kompatybilnym Unionem.
- Zweryfikowano Gothic II 2.6 (fix) i Union 1.0M. Inne konfiguracje wymagają osobnych testów.
- Windows PowerShell 5.1 lub PowerShell 7 do klienta; skrypty testowe wymagają PowerShell 7.
- Gotowa DLL nie wymaga kompilatora, CMake, Ninja ani Git.

## Szybki start

1. Pobierz i rozpakuj [wydanie v0.2.0](https://github.com/Scammers7531/ZenGin-DevBridge/releases/download/v0.2.0/ZenGinDevBridge-0.2.0-win32.zip).
2. Zamknij grę i wykonaj kopię zapasową konfiguracji Union oraz dotychczasowej DLL, jeśli istnieje.
3. Skopiuj `ZenGinDevBridge.dll` do `<game-root>\System\Autorun\`.
4. Uruchom grę i wczytaj świat. Zaczekaj na utworzenie katalogu wymiany `<game-root>\ZenGinDevBridge\`.
5. Otwórz PowerShell w rozpakowanym katalogu paczki i dostosuj ścieżkę gry:

```powershell
$client = '.\client\powershell\Query-DevBridge.ps1'
$gameRoot = 'C:\Games\Gothic II'
& $client -GameRoot $gameRoot -Command ping
& $client -GameRoot $gameRoot -Command player_state
```

Sprawdź `ok`, `plugin_version`, `protocol_version`, `world_ready` oraz `capabilities` odpowiedzi na `ping` przed kolejnymi zapytaniami. Katalog wymiany musi być zapisywalny; klient może też przyjąć parametr `-ExchangeDirectory`.

## Przykłady użycia

```powershell
& $client -GameRoot $gameRoot -Command vob -Name PC_HERO
& $client -GameRoot $gameRoot -Command vob_search -PayloadJson '{"class_contains":"zCMover","limit":20}'
& $client -GameRoot $gameRoot -Command waypoint_search -PayloadJson '{"limit":20}'
& $client -GameRoot $gameRoot -Command daedalus_read -PayloadJson '{"symbol":"TRUE","count":1}'
& $client -GameRoot $gameRoot -Command screenshot
```

Pełną odpowiedź JSON można zapisać przez `-SaveJsonPath`; eksport CSV jest przeznaczony dla `raycast_batch` i `surface_grid`.

```powershell
$payload = @{ default_flags = 1058; probes = @(@{ start = @(100,1000,200); direction = @(0,-1500,0) }) } | ConvertTo-Json -Depth 4 -Compress
& $client -GameRoot $gameRoot -Command raycast_batch -PayloadJson $payload -SaveJsonPath '.\raycasts.json' -ExportCsvPath '.\raycasts.csv'
```

## Agenci AI

Codex, Claude Code i inne narzędzia z dostępem do lokalnego terminala mogą wywoływać klienta PowerShell i analizować jego odpowiedzi. Nie jest wymagany serwer MCP. Przykładowe polecenie dla agenta:

> Sprawdź wersję i możliwości przez ping. Odczytaj player_state i player_collision oraz wyszukaj pobliskie obiekty. Zinterpretuj dane, odróżnij brak danych od wartości zerowych i nie zmieniaj stanu gry.

Klient lub agent wysyłający JSON bezpośrednio powinien serializować żądania, stosować unikalne identyfikatory i aktualne terminy oraz czekać na pasującą odpowiedź. Zobacz [API](docs/API.md).

## Konfiguracja i bezpieczeństwo

Opcjonalny plik `ZenGinDevBridge.config.json` umieść w katalogu głównym gry obok folderu `System`.

```json
{
  "config_version": 1,
  "exchange_directory": "ZenGinDevBridge",
  "allow_camera_control": false,
  "allow_scenario_start": false,
  "allow_player_control": false
}
```

Domyślnie operacje ustawiania kamery, wysyłania scenariuszy i sterowania bohaterem są wyłączone. `camera_restore` pozostaje dostępne jako operacja przywracania; `player_state` oraz `player_collision` są diagnostyczne. `screenshot` zapisuje plik obrazu lokalnie.

Po zmianie konfiguracji uruchom grę ponownie. Operacje zmieniające stan wykonuj tylko za zgodą użytkownika w odpowiedniej instalacji testowej. Przykład po włączeniu `allow_player_control`, sprawdzeniu możliwości i zakończeniu dialogów:

```powershell
& $client -GameRoot $gameRoot -Command player_move -PayloadJson '{"direction":"forward","duration_ms":1000}'
```

Sprawdź rzeczywisty dystans i `input_released`; samo wykonanie żądania nie oznacza ruchu. Wysyłanie scenariuszy wymaga osobnej konfiguracji mapowania symboli opisanej w [API](docs/API.md#konfiguracja-lokalna).

## Znane ograniczenia

- `player_walk_to` nie omija przeszkód; trasy WayNetu i sondy kolizji nie gwarantują fizycznej przechodniości.
- Chodzenie do przodu i dojście do celów potwierdzono. Sterowanie boczne pozostaje eksperymentalne: w sprawdzonej konfiguracji wejście pluginu nie zmieniało X/Z, choć klawiatura działała.
- `dialogue.active` może być fałszywe podczas widocznego dialogu. Obsługa menu, zapisywanie/wczytywanie i wybór dialogów nie należą do tego wydania.
- Część skanów ma ograniczony budżet czasu i może zwrócić niepełne wyniki. Inspekcja moverów jest pasywna; niepotwierdzona pozostaje niepusta hierarchia VObów w testowanej scenie.
- Finalna DLL przeszła 11/11 kontroli runtime i 7/7 kontroli zgodności w izolowanym środowisku. Nie wykonano porównywalnego benchmarku wydajności finalnej DLL ani testów wszystkich konfiguracji Union.

## Kompilacja ze źródeł

Wymaga Visual Studio 2022 C++ Build Tools x86, Windows 10 SDK, CMake 3.24+, Ninja i Git. Dostosuj ścieżki narzędzi:

```powershell
git clone --recurse-submodules https://github.com/Scammers7531/ZenGin-DevBridge.git
cd ZenGin-DevBridge
git checkout v0.2.0
git submodule update --init --recursive
.\scripts\build.ps1 `
  -ToolchainRoot 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools' `
  -WindowsSdkRoot 'C:\Program Files (x86)\Windows Kits\10' `
  -CMakePath 'C:\Program Files\CMake\bin\cmake.exe' `
  -NinjaPath 'C:\Program Files\CMake\bin\ninja.exe'
```

Wynik: `build\g2a-release\ZenGinDevBridge.dll`. Skrypt nie instaluje DLL. Testy protokołu:

```powershell
pwsh -NoProfile -File .\tests\Invoke-MockProtocolTests.ps1
pwsh -NoProfile -File .\tests\Invoke-ClientLockTests.ps1
```

Testy runtime i sterowania wykonuj tylko w odizolowanej instalacji gry, po zakończeniu scen blokujących ruch. Szczegóły w [raporcie testów](docs/TESTBED-STATUS.md).

## Dokumentacja i licencja

- [Pełne API i konfiguracja](docs/API.md)
- [Dziennik zmian](CHANGELOG.md)
- [Środowisko testowe i wyniki](docs/TESTBED-STATUS.md)
- [Licencja MIT](LICENSE) i [informacje o zależnościach](THIRD_PARTY_NOTICES.md)

Paczka wydania v0.2.0 pozostaje niezmieniona; aktualne poprawki redakcyjne dokumentacji są dostępne w gałęzi `main`.
