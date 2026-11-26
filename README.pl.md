[English](README.md) | [Polski](README.pl.md)

# ZenGin DevBridge

ZenGin DevBridge to lokalny plugin diagnostyczny oparty na wymianie plików dla **Gothic II: Noc Kruka (x86)**. Umożliwia lokalnym skryptom i agentom AI odczytywanie stanu załadowanego świata przez Union API, bez uruchamiania serwera sieciowego.

Wersja `0.1.0` jest publicznym wydaniem preview. Jest przeznaczona dla modderów, którzy potrafią kompilować i testować pluginy Union oraz rozumieją, że gra jest działającym procesem.

## Dostępne narzędzia

- Odczyt klasy VOba, transformacji, granic przestrzennych i flag kolizji.
- Wykonywanie pojedynczego raycastu lub ograniczonej liczby raycastów w jednej serii.
- Próbkowanie ograniczonej siatki X/Z `surface_grid`, z porównaniem trafień w świat statyczny, dynamiczne VOby i bounding boxy oraz odczytami podłoża, wody i sufitu dla gracza.
- Odczyt transformacji kamery oraz jej flag AI.
- Opcjonalne ustawianie i przywracanie kamery. Zmiany kamery są domyślnie wyłączone.
- Opcjonalne wysyłanie żądań i odczyt skonfigurowanych symboli całkowitoliczbowych Daedalusa. Obsługa scenariuszy jest domyślnie wyłączona i nie zawiera wbudowanych nazw scen.
- Odczyt wersji protokołu i możliwości komend za pomocą `ping`.

Domyślnie dostępne są komendy tylko do odczytu. `camera_set` i `scenario_start` zmieniają stan gry i wymagają lokalnego włączenia. `camera_restore` jest zawsze dostępne, aby usunąć nadpisanie kamery ustawione przez DevBridge.

## Wymagania

- Windows 10 lub nowszy.
- Gothic II: Noc Kruka w wersji x86 z kompatybilną instalacją Union.
- Visual Studio 2022 C++ x86 Build Tools i Windows 10 SDK.
- CMake 3.24 lub nowszy, Ninja i Git.

Projekt kompiluje się z przypiętymi podmodułami Union API i Gothic API. Nie zawiera prywatnych materiałów projektowych ani zasobów gry.

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
   .\client\powershell\Query-DevBridge.ps1 -GameRoot 'C:\Games\Gothic II'
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
  "allow_scenario_start": false
}
```

Ustaw `allow_camera_control` na `true` tylko wtedy, gdy chcesz zezwolić na `camera_set`. Przywracanie kamery jest zawsze dozwolone. Wysyłanie scenariusza wymaga także lokalnego mapowania `scenario_dispatch`; zobacz [opis API i konfiguracji](docs/API.pl.md#konfiguracja-lokalna). Pozostaw ten plik lokalnie i przed włączeniem sprawdź każde mapowanie symbolu.

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

Pełny format żądań, pola odpowiedzi, błędy i zabezpieczenia operacji zmieniających stan opisuje [docs/API.pl.md](docs/API.pl.md).

### Codex i Claude Code

Codex, Claude Code oraz inni agenci programistyczni mogą uruchamiać dołączonego klienta PowerShell w swoim terminalu i analizować zwrócony JSON. Przykład polecenia dla agenta, które nie zmienia stanu gry:

```text
Użyj klienta PowerShell ZenGin DevBridge. Najpierw wywołaj ping i sprawdź world_ready oraz capabilities. Następnie sprawdź PC_HERO komendą vob i zbadaj podłoże za pomocą surface_grid. Opisz wyniki JSON. Nie wywołuj camera_set ani scenario_start.
```

Jeśli jawnie zezwolisz na zmianę kamery, poproś agenta o sprawdzenie, czy `camera_set` jest dostępne, wykonanie żądanej operacji i późniejsze wywołanie `camera_restore`. Wysłanie scenariusza wymaga osobnej, jawnej zgody oraz sprawdzonego lokalnego mapowania symbolu. Przewodnik API opisuje ten sam sposób pracy dla klientów wysyłających pliki JSON bezpośrednio.

## Kompatybilność i wsparcie

- To wydanie preview obsługuje wyłącznie Gothic II: Noc Kruka.
- Testy w odizolowanym środowisku wykonano z Gothic II 2.6 (fix) i Union 1.0M.
- Dla zachowania zgodności starsze klienty mogą pomijać `protocol_version`; takie żądania są traktowane jako wersja 1.
- Nazwy scenariuszy i symbole Daedalusa zależne od środowiska należą do lokalnej konfiguracji, a nie do publicznego kodu pluginu.
- Przy zweryfikowanej kompilacji lub późniejszym wdrożeniu zapisuj rewizję źródeł, wersję pluginu i SHA-256 DLL.

## Podstawowe testy protokołu

Dołączony test protokołu sprawdza, czy klient PowerShell wysyła wersję protokołu 1 i termin wygaśnięcia przypadający w przyszłości, obsługuje odpowiedź z pasującym identyfikatorem oraz zapisuje znacznik anulowania po przekroczeniu limitu czasu:

```powershell
pwsh -NoProfile -File .\tests\Invoke-MockProtocolTests.ps1
```

Test sprawdza plikowy przepływ klienta. Testy działania w silniku oraz krótki pomiar bezczynności wykonano również w odizolowanym środowisku testowym; zakres i ograniczenia opisuje [docs/TESTBED-STATUS.md](docs/TESTBED-STATUS.md). Wyniki mogą się różnić w zależności od instalacji Gothica, wersji Union i moda.

## Licencja

Kod źródłowy DevBridge jest udostępniany na licencji MIT. Informacje o kodzie stron trzecich i wymagane treści licencji znajdują się w [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Projekt nie rozpowszechnia szablonu pluginu Union używanego na wczesnym etapie prac; repozytorium tego szablonu nie publikuje licencji, dlatego jego pliki bazowe i pozostałe materiały zostały wykluczone.
