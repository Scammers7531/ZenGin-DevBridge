[English](API.md) | [Polski](API.pl.md)

# Plikowe API DevBridge

Wersja protokołu: **1**. Jest to lokalny protokół plikowy obsługujący jedno żądanie naraz. Plugin odczytuje żądania w ramach hooka renderującego Gothica; nie otwiera portu sieciowego.

## Pliki wymiany

Domyślnym katalogiem wymiany jest `<game-root>\ZenGinDevBridge`. Lokalna konfiguracja może wskazać inną pojedynczą nazwę katalogu względem katalogu głównego gry.

| Plik | Przeznaczenie |
| --- | --- |
| request.json | Jeden obiekt żądania UTF-8, zapisywany przez klienta atomowo. |
| response.json | Odpowiedź na pasujące żądanie. Klient musi porównać `id`. |
| cancel.<id> | Pusty znacznik informujący plugin, by przerwał obsługę tego żądania. |
| plugin-heartbeat.json | Wersja pluginu i protokołu, status hooka oraz poprawność konfiguracji. |
| diagnostics.jsonl | Dziennik dopisywanych kodów błędów, z `id` i komendą, jeśli są dostępne; bez ścieżek do pliku wykonywalnego ani katalogu roboczego. |
| client.lock | Blokada używana przez dołączonego klienta PowerShell do szeregowania własnych żądań. |

Wysyłaj jedno żądanie naraz. Plugin sprawdza identyfikator żądania przed użyciem go w nazwie pliku anulowania. Żądania większe niż 64 KiB są odrzucane. Plugin publikuje odpowiedzi przez atomową podmianę pliku tymczasowego. Po błędzie dysku można ponowić zapis odpowiedzi bez ponownego wykonania komendy.

## Koperta żądania

    {
      "id": "agent-0001",
      "command": "ping",
      "protocol_version": 1,
      "expires_at_unix_ms": 1795700000000
    }

- `id`: wymagane; 1–80 znaków ASCII: litery, cyfry, kropka, podkreślenie lub łącznik. Same wartości `.` i `..` są niedozwolone.
- `command`: wymagane; maksymalnie 64 znaki.
- `protocol_version`: opcjonalne dla starszych klientów; jeśli go brakuje, żądanie jest traktowane jako wersja 1. Inna wersja powoduje błąd `protocol_version_unsupported`.
- `expires_at_unix_ms`: zalecany bezwzględny termin w milisekundach Unix. Maksymalny akceptowany czas do wygaśnięcia wynosi 180 sekund. Starsze klienty mogą pominąć to pole; plugin użyje wtedy czasu modyfikacji pliku żądania i limitu wieku wynoszącego 120 sekund.

Po upływie terminu klient powinien utworzyć `cancel.<id>` i pozostawić plik żądania na tyle długo, by plugin mógł zauważyć anulowanie i opublikować wynik. Klient nie może usuwać ani zastępować żądania, gdy plugin może je jeszcze obsługiwać. Dołączony klient czeka do trzech sekund na potwierdzenie anulowania.

## Koperta odpowiedzi i błędy

Każda odpowiedź zawiera `id`, `command`, `protocol_version`, `plugin_version` i `ok`. Odpowiedź błędu zawiera również:

    {
      "ok": false,
      "error_code": "world_not_ready",
      "error": "no world is currently loaded"
    }

Starsze pole tekstowe `error` jest zachowane. Klient powinien podejmować decyzje na podstawie `error_code`.

| Kod błędu | Znaczenie |
| --- | --- |
| invalid_request | Nieprawidłowe pola JSON, typy, zakresy, wektor, flagi albo termin. |
| protocol_version_unsupported | Klient zażądał nieobsługiwanej wersji protokołu. |
| timeout | Żądanie wygasło przed zakończeniem obsługi. |
| cancelled | Klient anulował żądanie. |
| world_not_ready | Żaden świat gry nie jest załadowany. |
| world_changed | Świat zmienił się podczas skanowania seriami; wyniki zostały odrzucone. |
| capability_disabled | Komenda zmieniająca stan lub korzystająca z konfiguracji jest wyłączona albo nie została skonfigurowana. |
| unknown_command | Nazwa komendy nie jest obsługiwana. |
| scan_failed, parser_unavailable, configured_symbol_missing, scenario_queue_busy, camera_unavailable, target_vob_not_found | Błąd właściwy dla danej komendy; szczegóły znajdują się w polu `error`. |
| internal_error | Nieoczekiwany błąd pluginu; sprawdź `diagnostics.jsonl`. |

## Możliwości

`ping` zwraca wersje pluginu i protokołu, informację, czy świat jest załadowany, oraz zestaw możliwości. Możliwości rozdzielają komendy `read_only` od komend zmieniających stan. Wartość `true` oznacza, że komenda jest włączona lub skonfigurowana; świat albo kamera nadal mogą być niedostępne w chwili wywołania.

    {
      "plugin": "ZenGinDevBridge",
      "plugin_version": "0.1.0",
      "protocol_version": 1,
      "world_ready": true,
      "capabilities": {
        "read_only": {
          "ping": true,
          "vob": true,
          "raycast": true,
          "raycast_batch": true,
          "surface_grid": true,
          "camera_get": true,
          "scenario_status": false
        },
        "mutating": {
          "camera_set": false,
          "scenario_start": false,
          "camera_restore": true
        }
      }
    }

## Komendy

### ping

Nie wymaga dodatkowych pól. Zwraca tożsamość i wersję pluginu, `world_ready`, pole `stage` zachowane dla starszych klientów oraz możliwości.

### vob

    {"command":"vob","name":"PC_HERO"}

Zwraca `found`; jeśli VOb został znaleziony, odpowiedź zawiera także `name`, `class`, `matrix`, `bbox3d_min`, `bbox3d_max`, flagi kolizji i `collision_object_present`.

### raycast

    {"command":"raycast","start":[100,1000,200],"direction":[0,-1500,0],"flags":1058}

`start` i `direction` to skończone wektory `[x,y,z]`, których składowe bezwzględne nie przekraczają 10 000 000. `flags` jest liczbą całkowitą z zakresu 0–32 767. Wynik zawiera informację `hit`, długość promienia, pozycję/wysokość/normalną trafienia, rodzaj geometrii oraz — jeśli są dostępne — dane materiału i VOba.

### raycast_batch

    {
      "command":"raycast_batch",
      "default_flags":1058,
      "probes":[
        {"start":[100,1000,200],"direction":[0,-1500,0]},
        {"start":[120,1000,200],"direction":[0,-1500,0],"flags":1058}
      ]
    }

Przyjmuje od 1 do 256 próbek. Plugin wykonuje maksymalnie 16 raycastów na klatkę i zwraca wyniki w tej samej kolejności.

### surface_grid

    {
      "command":"surface_grid",
      "center":[100,1000,200],
      "radius_x":20,
      "radius_z":20,
      "spacing":20,
      "down_range":2000,
      "include_bbox":false
    }

Wartości `radius_x` i `radius_z` mieszczą się w zakresie 0–5 000, `spacing` w zakresie 10–5 000, a `down_range` w zakresie 10–100 000. Wygenerowana siatka jest ograniczona do 121 próbek. Plugin obsługuje maksymalnie 3 próbki na klatkę. Każdy wiersz porównuje raycasty do geometrii wizualnej, świata statycznego i dynamicznych VObów oraz odczyty podłoża, wody i sufitu dla gracza. `include_bbox` dodaje raycast do bounding boxa VOba.

### camera_get

Nie wymaga dodatkowych pól. Odczytuje transformacje kamery, dane jej VOba oraz — jeśli są dostępne — tryb i flagi AI kamery.

### camera_set — komenda zmieniająca stan, domyślnie wyłączona

    {"command":"camera_set","position":[100,300,400],"look_at":[0,100,0]}

Podaj `look_at` jako skończony wektor albo `target_vob` jako nazwę VOba. Pierwsza zmiana zapisuje pierwotną transformację i flagi AI kamery. Kolejne aktualizacje zachowują ten sam punkt przywracania. Plugin stosuje transformację tylko wtedy, gdy aktywne pozostają ten sam świat i ta sama kamera.

### camera_restore — komenda przywracania

Nie wymaga dodatkowych pól. Jest zawsze akceptowana, nawet gdy sterowanie kamerą jest wyłączone. Odpowiedź podaje `restored`, `camera_ai_restored` i `world_matched`. Jeśli świat lub kamera zostały zastąpione albo są niedostępne, komenda nie odwołuje się do zapisanych wskaźników gry.

### scenario_start — komenda zmieniająca stan, domyślnie wyłączona

    {"command":"scenario_start","scenario":"example_step"}

Wysyłanie scenariusza działa tylko po lokalnym włączeniu i skonfigurowaniu. Plugin zapisuje skonfigurowaną liczbę całkowitą do wskazanego symbolu Daedalusa; nie wywołuje funkcji parsera z hooka renderującego. Wartość musi mieścić się w zakresie 1–32 767. Nazwa scenariusza jest kluczem lokalnego mapowania i nie jest wbudowana w publiczny plugin.

### scenario_status — odczyt oparty na konfiguracji

Nie wymaga dodatkowych pól. Odczytuje skonfigurowane symbole całkowitoliczbowe Daedalusa. Odpowiedź zawiera obiekt `scenario_status` oraz jego płaskie aliasy zgodne z lokalnie skonfigurowanymi kluczami, dzięki czemu istniejące klienty testowe mogą zachować oczekiwane pola odpowiedzi.

## Konfiguracja lokalna

Utwórz `<game-root>\ZenGinDevBridge.config.json` tylko wtedy, gdy potrzebujesz zmienić domyślny katalog wymiany lub włączyć komendę zmieniającą stan. Plik nie może przekraczać 16 KiB i obsługuje następujący format:

    {
      "config_version": 1,
      "exchange_directory": "ZenGinDevBridge",
      "allow_camera_control": false,
      "allow_scenario_start": false,
      "scenario_dispatch": {
        "request_symbol": "BRIDGE_SCENARIO_REQUEST",
        "scenarios": {"example_step": 1},
        "status_symbols": {"queue_value": "BRIDGE_QUEUE_VALUE"}
      }
    }

`exchange_directory` musi być pojedynczą nazwą katalogu w katalogu głównym gry; ścieżki bezwzględne i przejścia do katalogu nadrzędnego są odrzucane. Zmiany kamery i scenariuszy są domyślnie wyłączone. Nazwy scenariuszy, identyfikatory i symbole Daedalusa przechowuj w tym lokalnym pliku; nie dodawaj mapowań zależnych od środowiska do repozytorium. Jeśli pliku nie można odczytać lub zweryfikować, możliwości zmieniające stan są wyłączane, a przyczyna jest raportowana przez `ping` i heartbeat.

## Zalecany sposób użycia przez agenta

1. Wywołaj `ping` i sprawdź `protocol_version`, `world_ready` oraz `capabilities`.
2. Do obserwacji używaj `vob`, `raycast`, `raycast_batch` lub `surface_grid`.
3. Przed `camera_set` lub `scenario_start` sprawdź, czy dana możliwość zmieniająca stan ma wartość `true`, oraz uzyskaj jawną zgodę użytkownika lub systemu wywołującego.
4. Używaj unikalnego identyfikatora żądania, ustaw termin ważności i czekaj na odpowiedź z tym samym `id`.
5. Po przekroczeniu limitu czasu przez klienta zapisz `cancel.<id>` i zaczekaj na odpowiedź potwierdzającą anulowanie przed wysłaniem kolejnego żądania.
6. Po zmianie kamery wywołaj `camera_restore` i sprawdź wynik w polu `restored`.

Aby ustawić aktualny termin, w chwili tworzenia żądania oblicz czas Unix w milisekundach i dodaj do niego wybrany limit czasu. Nie używaj ponownie przykładowego terminu z tej dokumentacji.
