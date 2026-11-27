[🇵🇱 Polski](API.md) | [🇬🇧 English](API.en.md)

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
| client.lock | Blokada tworzona na wyłączność przez dołączonego klienta PowerShell; zawiera identyfikator jej właściciela. |

Wysyłaj jedno żądanie naraz. Plugin sprawdza identyfikator żądania przed użyciem go w nazwie pliku anulowania. Żądania większe niż 64 KiB są odrzucane. Plugin publikuje odpowiedzi przez atomową podmianę pliku tymczasowego. Po błędzie zapisu odpowiedzi ponawia jej publikację bez ponownego wykonania komendy.

`client.lock` koordynuje wyłącznie równoległe uruchomienia dołączonego klienta PowerShell. Klient usuwa plik tylko wtedy, gdy sam go utworzył i nadal zawiera on jego identyfikator. Plugin ani zewnętrzni klienci protokołu nie używają tej blokady, więc powinni samodzielnie serializować żądania. Jeśli klient zakończy się awaryjnie i pozostawi plik blokady, przed usunięciem go ręcznie upewnij się, że żaden klient DevBridge nie działa.

### Klient PowerShell

Dołączony klient działa w Windows PowerShell 5.1 i PowerShell 7; skrypty testowe wymagają PowerShell 7 (pwsh). Parametr -SaveJsonPath zapisuje całą odpowiedź JSON. Parametr -ExportCsvPath eksportuje wiersze results tylko dla raycast_batch i surface_grid.

    $client = '.\client\powershell\Query-DevBridge.ps1'
    $gameRoot = 'C:\Games\Gothic II'
    $payload = @{ default_flags = 1058; probes = @(@{ start = @(100, 1000, 200); direction = @(0, -1500, 0) }) } | ConvertTo-Json -Depth 4 -Compress
    & $client -GameRoot $gameRoot -Command raycast_batch -PayloadJson $payload -SaveJsonPath '.\raycasts.json' -ExportCsvPath '.\raycasts.csv'

## Koperta żądania

    {
      "id": "agent-0001",
      "command": "ping",
      "protocol_version": 1,
      "expires_at_unix_ms": 1795700000000
    }

- `id`: wymagane; 1–80 znaków ASCII: litery, cyfry, kropka, podkreślenie lub łącznik. Same wartości `.` i `..` są niedozwolone.
- `command`: wymagane; od 1 do 64 bajtów tekstu UTF-8, bez znaku NUL. Nieznana, ale poprawna składniowo nazwa otrzymuje odpowiedź `unknown_command`.
- `protocol_version`: opcjonalne dla starszych klientów; jeśli go brakuje, żądanie jest traktowane jako wersja 1. Inna wersja powoduje błąd `protocol_version_unsupported`.
- `expires_at_unix_ms`: zalecany bezwzględny termin w milisekundach Unix. Maksymalny akceptowany czas do wygaśnięcia wynosi 180 sekund. Starsze klienty mogą pominąć to pole; plugin użyje wtedy czasu modyfikacji pliku żądania i limitu wieku wynoszącego 120 sekund.

Gdy termin wygaśnie, plugin zwraca `timeout`, o ile wcześniej nie zauważy znacznika anulowania. Klient może utworzyć `cancel.<id>`; plugin sprawdza go na bezpiecznych punktach kontrolnych, między innymi podczas skanowania wieloklatkowego. Jeśli znacznik i termin zostaną zauważone w tej samej kontroli, pierwszeństwo ma `cancelled`. Anulowanie nie przerywa natychmiast trwającego wywołania silnika. `camera_restore` nie reaguje na znacznik anulowania, ale żądanie musi nadal mieć poprawną kopertę i nieprzekroczony termin.

Dołączony klient po osiągnięciu własnego limitu czasu zapisuje znacznik tylko wtedy, gdy `request.json` nadal zawiera jego `id`, a następnie czeka do trzech sekund na odpowiedź. Nie usuwa aktywnego żądania. Jeśli nie otrzyma potwierdzenia w tym czasie, kończy pracę z błędem i pozostawia stan plików do sprawdzenia; nie wysyłaj kolejnego żądania, dopóki poprzednie nie zostanie obsłużone lub ręcznie wyjaśnione.

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
| invalid_request | Nieprawidłowe pole żądania, typ, zakres, wektor, flaga lub termin. Używany dla poprawnie odczytanej koperty; część błędów strukturalnych nie otrzymuje odpowiedzi — zobacz niżej. |
| protocol_version_unsupported | Żądana wersja protokołu nie jest obsługiwana; odpowiedź zawiera `supported_protocol_versions`. |
| request_file_error | Plugin nie może odczytać czasu modyfikacji pliku dla starszego żądania bez `expires_at_unix_ms`. |
| timeout | Termin żądania minął przed rozpoczęciem lub zakończeniem obsługi. |
| cancelled | Plugin zauważył `cancel.<id>` na punkcie kontrolnym przed upływem terminu. |
| world_not_ready | Świat gry lub wymagany parser nie jest dostępny. |
| world_changed | Świat zmienił się podczas skanowania wieloklatkowego; częściowe wyniki odrzucono. |
| capability_disabled | Komenda zmieniająca stan lub korzystająca z konfiguracji jest wyłączona albo nie została skonfigurowana. |
| unknown_command | Nazwa komendy nie jest obsługiwana. |
| unsupported_scenario | Nazwa scenariusza nie występuje w lokalnej mapie `scenario_dispatch.scenarios`. |
| scan_failed | Ogólny błąd wykonania `raycast_batch` lub `surface_grid`; szczegóły są w `error`. |
| parser_unavailable | Parser Daedalusa jest niedostępny podczas obsługi scenariusza. |
| configured_symbol_missing | Skonfigurowanej nazwy symbolu nie znaleziono w parserze Daedalusa. |
| configured_symbol_unavailable | Parser znalazł symbol, ale nie zwrócił dostępnego obiektu symbolu. |
| configured_symbol_type_mismatch | Symbol skonfigurowany do odczytu lub zapisu nie jest skalarną zmienną całkowitą Daedalusa. |
| configured_symbol_not_writable | Symbol kolejki scenariuszy jest stałą i nie można do niego zapisać. |
| scenario_queue_busy | Skonfigurowany symbol kolejki ma już niezerową wartość, więc poprzednie żądanie nie zostało jeszcze odebrane. |
| camera_unavailable | Kamera lub jej VOb nie są dostępne. |
| target_vob_not_found | Nie znaleziono VOba wskazanego przez `target_vob` w `camera_set`. |
| internal_error | Nieoczekiwany wyjątek pluginu; sprawdź `diagnostics.jsonl`. |

### Odrzucenia bez `response.json`

Plugin publikuje odpowiedź tylko wtedy, gdy potrafi odczytać poprawny obiekt JSON z bezpiecznym `id` i `command`. Uszkodzony JSON, brak tekstowego `id` lub `command` są odnotowywane jako `malformed_request`. Nieprawidłowy format `id` albo puste `command`, ponad 64 bajty UTF-8 lub zawierające NUL są rejestrowane jako `invalid_request_identity`. W obu przypadkach żądanie jest odrzucane bez odpowiedzi. Nieznana nazwa komendy, która spełnia ograniczenia składni, zwraca `unknown_command`. Plik większy niż 64 KiB jest rejestrowany jako `request_too_large` i usuwany.

Błędy dostępu do katalogu lub plików (`exchange_directory_unavailable`, `request_file_unavailable`, `request_open_failed`) oraz błędy publikacji odpowiedzi (`response_write_failed`, `response_request_mismatch`) również mogą oznaczać brak nowej odpowiedzi. Przy błędzie publikacji plugin ponawia zapis, jeśli nadal obsługuje to samo żądanie. Zawsze sprawdzaj zgodność `id` w `response.json` i w razie problemu analizuj `diagnostics.jsonl`; znajdujący się tam wcześniejszy plik odpowiedzi nie jest potwierdzeniem bieżącego żądania.

### Kody diagnostyczne w `diagnostics.jsonl`

Każdy kod błędu zwrócony w `error_code` jest również zapisywany diagnostycznie. Poniższe kody dotyczą warstwy plików, hooka lub heartbeat i same w sobie nie są odpowiedzią protokołu:

| Kod diagnostyczny | Znaczenie |
| --- | --- |
| exchange_directory_unavailable | Plugin nie może utworzyć ani uzyskać dostępu do katalogu wymiany. |
| request_file_unavailable | Nie można sprawdzić pliku żądania. |
| request_open_failed | Nie można otworzyć pliku żądania; plugin może ponowić próbę przy następnym odpytywaniu. |
| request_too_large | Żądanie przekracza 64 KiB; plik jest usuwany bez odpowiedzi. |
| malformed_request | JSON lub koperta bez poprawnych tekstowych pól `id` i `command` są nieprawidłowe; plik jest odrzucany bez odpowiedzi. |
| invalid_request_identity | `id` nie spełnia wzorca albo `command` jest puste, dłuższe niż 64 bajty UTF-8 lub zawiera NUL; plik jest odrzucany bez odpowiedzi. |
| protocol_version_unsupported | Nieobsługiwana wersja protokołu; poza wpisem diagnostycznym klient otrzymuje odpowiedź z `error_code`. |
| response_write_failed | Publikacja odpowiedzi nie powiodła się; plugin ponawia zapis dla aktywnego żądania. |
| response_request_mismatch | `request.json` nie zawiera już obsługiwanego `id`; plugin nie publikuje nieaktualnej odpowiedzi. |
| heartbeat_write_failed | Nie udało się zapisać tymczasowego pliku heartbeat. |
| heartbeat_publish_failed | Nie udało się opublikować pliku heartbeat. |
| heartbeat_error | Nieoczekiwany wyjątek podczas tworzenia heartbeat. |
| render_hook_processing_error | Nieoczekiwany wyjątek poza obsługą pojedynczego żądania w hooku renderującym. |

## Możliwości

ping zwraca wersje pluginu i protokołu, informację world_ready oraz capabilities. Pole read_only opisuje komendy obserwacyjne; mutating podaje dostępność operacji zmieniających stan. Wartość true oznacza, że możliwość jest włączona lub skonfigurowana, ale nie gwarantuje dostępności świata, kamery ani celu w chwili wywołania. player_control w grupie mutating jest flagą zbiorczą, a nie komendą.

Przykładowa odpowiedź przy domyślnej konfiguracji:

    {
      "plugin": "ZenGinDevBridge",
      "plugin_version": "0.2.0",
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
          "vob_search": true,
          "vob_hierarchy": true,
          "npc_search": true,
          "npc_inspect": true,
          "player_state": true,
          "player_collision": true,
          "waypoint_search": true,
          "waypoint_inspect": true,
          "waynet_local_graph": true,
          "waynet_nearest": true,
          "waynet_check": true,
          "waynet_route": true,
          "daedalus_search": true,
          "daedalus_read": true,
          "mover_inspect": true,
          "monitor": true,
          "screenshot": true,
          "scenario_status": false
        },
        "mutating": {
          "camera_set": false,
          "scenario_start": false,
          "player_teleport": false,
          "player_turn": false,
          "player_move": false,
          "player_walk_to": false,
          "player_control": false,
          "camera_restore": true
        },
        "config_valid": true,
        "camera_control_enabled": false,
        "player_control_enabled": false,
        "scenario_dispatch_enabled": false
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

Współrzędne są pozycjami świata Gothic w surowych jednostkach silnika; plugin nie przelicza ich na metry. Osie X i Z leżą poziomo, a Y jest osią pionową. direction jest wektorem przesunięcia, nie wektorem jednostkowym: koniec promienia to start + direction, a długość wynika z długości tego wektora. flags jest maską bitową przekazywaną do zCWorld::TraceRayNearestHit:

| Wartość | Flaga |
| ---: | --- |
| 1 | zTRACERAY_VOB_IGNORE_NO_CD_DYN |
| 2 | zTRACERAY_VOB_IGNORE |
| 4 | zTRACERAY_VOB_BBOX |
| 8 | zTRACERAY_VOB_OBB |
| 16 | zTRACERAY_STAT_IGNORE |
| 32 | zTRACERAY_STAT_POLY |
| 64 | zTRACERAY_STAT_PORTALS |
| 128 | zTRACERAY_POLY_NORMAL |
| 256 | zTRACERAY_POLY_IGNORE_TRANSP |
| 512 | zTRACERAY_POLY_TEST_WATER |
| 1024 | zTRACERAY_POLY_2SIDED |
| 2048 | zTRACERAY_VOB_IGNORE_CHARACTER |
| 4096 | zTRACERAY_FIRSTHIT |
| 8192 | zTRACERAY_VOB_TEST_HELPER_VISUALS |
| 16384 | zTRACERAY_VOB_IGNORE_PROJECTILES |

Wartość 1058 łączy zTRACERAY_VOB_IGNORE (2), zTRACERAY_STAT_POLY (32) i zTRACERAY_POLY_2SIDED (1024).

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

`surface_grid` i `character_support` dostarczają pomiary geometrii oraz bieżący odczyt obiektu kolizji gracza. Nie symulują chodzenia postaci: nie przewidują trasy, przechodzenia po schodach, skoków, animacji ani reakcji AI. Używaj ich do diagnostyki powierzchni i kolizji, nie jako testu pełnej przechodniości lub nawigacji.

### camera_get

Nie wymaga dodatkowych pól. Odczytuje transformacje kamery, dane jej VOba oraz — jeśli są dostępne — tryb i flagi AI kamery.

### camera_set — komenda zmieniająca stan, domyślnie wyłączona

    {"command":"camera_set","position":[100,300,400],"look_at":[0,100,0]}

Podaj `look_at` jako skończony wektor albo `target_vob` jako nazwę VOba. Pierwsza zmiana zapisuje pierwotną transformację i flagi AI kamery. Kolejne aktualizacje zachowują ten sam punkt przywracania. Plugin stosuje transformację tylko wtedy, gdy aktywne pozostają ten sam świat i ta sama kamera. Po osiągnięciu celu wywołaj `camera_restore`.

### camera_restore — komenda przywracania

Nie wymaga dodatkowych pól i jest dostępna nawet wtedy, gdy sterowanie kamerą jest wyłączone. Żądanie nadal musi mieć poprawną kopertę i nieprzekroczony termin. Odpowiedź podaje `restored`, `camera_ai_restored` i `world_matched`. Transformacja jest przywracana tylko wtedy, gdy nadal aktywne są ten sam świat, kamera i VOb kamery, które zapisano przy pierwszym `camera_set`. Flagi AI są przywracane osobno, jeżeli pierwotny obiekt AI kamery nadal istnieje. Przy braku nadpisania albo po zmianie/niedostępności obiektów odpowiednie pola mają wartość `false`; punkt przywracania zostaje wtedy skasowany i nie można ponowić tej próby. Zmiana świata powoduje bezpieczne odrzucenie zapisanego punktu bez dereferencji starych wskaźników.

### scenario_start — komenda zmieniająca stan, domyślnie wyłączona

    {"command":"scenario_start","scenario":"example_step"}

Wysyłanie scenariusza działa tylko po lokalnym włączeniu i skonfigurowaniu. Plugin zapisuje skonfigurowaną liczbę całkowitą do wskazanego symbolu Daedalusa; nie wywołuje funkcji parsera z hooka renderującego. Wartość musi mieścić się w zakresie 1–32 767. Nazwa scenariusza jest kluczem lokalnego mapowania i nie jest wbudowana w publiczny plugin.

Plugin zapamiętuje ostatnią wysłaną parę nazwy i identyfikatora scenariusza w bieżącym świecie. Ponowne wysłanie tej samej pary zwraca `ok: true` oraz `already_dispatched: true`, ale nie zapisuje jej ponownie do symbolu kolejki — nawet jeśli mod zdążył już wyzerować ten symbol. Ponowienie będzie możliwe po skutecznym wysłaniu innego scenariusza albo po zmianie świata. Ten mechanizm chroni przed przypadkowym powtórzeniem, ale oznacza, że ponowne uruchomienie tego samego scenariusza w tym samym świecie wymaga zaplanowanego resetu poprzez inny scenariusz lub zmianę świata.

### scenario_status — odczyt oparty na konfiguracji

Nie wymaga dodatkowych pól. Odczytuje skonfigurowane symbole całkowitoliczbowe Daedalusa. Odpowiedź zawiera obiekt `scenario_status` oraz jego płaskie aliasy zgodne z lokalnie skonfigurowanymi kluczami, dzięki czemu istniejące klienty testowe mogą zachować oczekiwane pola odpowiedzi.

## Dodatkowe komendy diagnostyczne — kandydat v0.2.0

Ta sekcja opisuje lokalny, nieopublikowany kandydat. Nie jest to jeszcze publiczne wydanie. Protokół pozostaje w wersji 1; dotychczasowe komendy zachowują swoje formaty.

### Wyszukiwanie VObów

    {"command":"vob_search","name_contains":"CHEST","class_contains":"zCVob","center":[0,0,0],"radius":5000,"sort_by":"distance","limit":25,"offset":0,"scan_limit":20000}

Filtry opcjonalne: name_exact, name_contains i class_contains. Pozycję odniesienia podaj przez center albo center_vob, nigdy jednocześnie. radius (0–100000) wymaga punktu odniesienia; sort_by obsługuje tylko distance. limit ma zakres 1–100, offset 0–100000, a scan_limit 1–200000. Skan pojedynczego żądania ma wewnętrzny limit 15 ms. Wynik zawiera results, nodes_scanned, total_vobs, scan_truncated i scan_time_limited. offset stronicuje dopasowania z bieżącego skanu; API nie udostępnia kursora do wznowienia skanu przerwanego limitem czasu.

### Hierarchia VOba

Puste tablice ancestors i children oznaczają, że dla poprawnego węzła celu bieżący świat nie zwrócił relacji z przodkami lub dziećmi. Nie jest to błąd żądania ani dowód, że cały świat nie ma hierarchii.

    {"command":"vob_hierarchy","name":"PC_HERO","children_depth":3,"max_nodes":128}

name wskazuje dokładny VOb. children_depth ma zakres 0–8 (domyślnie 2), max_nodes 1–256 (domyślnie 64). Odpowiedź zawiera target, ancestors, children, children_nodes_scanned i children_truncated. Liczba przodków jest ograniczona do 64.

### Wyszukiwanie i inspekcja NPC

    {"command":"npc_search","name_contains":"GUARD","instance_contains":"MIL","guild_contains":"GUARD","center_vob":"PC_HERO","radius":5000,"limit":25,"offset":0,"scan_limit":20000}

npc_search filtruje po name_contains, instance_contains i guild_contains. center albo center_vob uruchamia sortowanie wyników według odległości; radius (0–100000) wymaga jednego z tych pól. limit 1–100, offset 0–100000, scan_limit 1–200000; filtr ma najwyżej 128 znaków. Skan ma limit czasu 15 ms. Odpowiedź sygnalizuje częściowy skan polami scan_truncated i scan_time_limited.

    {"command":"npc_inspect","name":"NPC_VOB_NAME","inventory_limit":8}

npc_inspect przyjmuje nazwę VOba NPC i inventory_limit 0–16 (domyślnie 8). Zwraca dostępne informacje o instancji, pozycji i orientacji, zdrowiu, stanie życia, gildii, AI, rutynie i bieżącej czynności, celu ruchu, walce, waypointcie, animacji, dialogu i ekwipunku. Informacje opcjonalne są pomijane albo oznaczane jako niedostępne; plugin nie wyprowadza ich z przypuszczeń.

### Stan bohatera i diagnostyka kolizji

player_state nie wymaga dodatkowych pól. Zwraca te same grupy informacji o bohaterze co npc_inspect, flagę player, dostępne parametry oCAIPlayer (ruch, prędkość, wysokość stóp i głowy, kontakt z podłożem, spadanie i poślizg), collision_support oraz pomocniczy ground_probe.

player_collision nie wymaga dodatkowych pól. Zwraca odczyt character_support, cztery raycasty przeszkód: przód i tył po 180 jednostek, lewo i prawo po 140 jednostek, oraz dostępny stan ruchu i kolizji. W odpowiedzi full_character_simulation ma wartość false: są to odczyty silnika i sondy, a nie pełna symulacja kolizji ani dowód, że postać może przejść daną trasą.

### WayNet

    {"command":"waypoint_search","name_contains":"WP","limit":64,"offset":0}

waypoint_search przyjmuje name_contains i/lub dokładne name; limit ma zakres 1–256, offset 0–100000. waypoint_inspect wymaga dokładnego name. Obie odpowiedzi opisują pozycję waypointa i jego dostępne połączenia.

    {"command":"waynet_nearest","position":[0,0,0]}

Alternatywnie waynet_nearest przyjmuje nazwę VOba w polu vob. waynet_local_graph wymaga center (nazwy waypointa), a opcjonalnie przyjmuje depth 0–8 (domyślnie 2) i max_nodes 1–256 (domyślnie 64). waynet_check przyjmuje component_scan_limit 1–100000 (domyślnie 100000); raportuje liczbę punktów i krawędzi, odizolowane punkty, krawędzie bez celu oraz próbkę słabo spójnych składowych. Listy próbek są ograniczone.

    {"command":"waynet_route","start":"WP_START","target":"WP_END","max_nodes":20000}

waynet_route wykonuje ograniczony algorytm Dijkstry po krawędziach WayNetu. Używa dodatniego kosztu krawędzi, a gdy go brak, odległości euklidesowej między punktami. max_nodes ma zakres 1–100000. Odpowiedź jawnie zwraca algorithm i physical_walkability_verified:false. Trasa grafowa nie jest wynikiem nawigacji AI ani gwarancją przejścia w świecie.

### Odczyt Daedalusa i inspekcja movera

daedalus_read może odczytywać także stałe. Zmienne klasowe zwracają wyłącznie metadane z value_available:false, ponieważ klasa wymaga aktywnego kontekstu instancji. Odczyt tablic obejmuje tylko zakres wskazany przez start_index i count.

    {"command":"daedalus_search","name_contains":"TEST","limit":64,"offset":0,"scan_limit":20000}

daedalus_search zwraca metadane symboli; limit 1–256, offset 0–100000, scan_limit 1–100000, filtr do 128 znaków. Skan jest ograniczony do 15 ms. daedalus_read wymaga symbol i opcjonalnie przyjmuje start_index 0–4095 oraz count 1–256. Odczyt obejmuje int, float i string, również tablice w żądanym zakresie. Ciągi są obcinane po 4096 bajtach. Typy nieobsługiwane oraz zmienne klasowe otrzymują value_available:false i value_reason. Plugin nie zapisuje dowolnych symboli Daedalusa.

    {"command":"mover_inspect","name":"MOVER_VOB_NAME"}

mover_inspect tylko odczytuje stan obiektu zCMover, parametry ruchu, pozycję i do 64 klatek kluczowych. related_target jest wyłącznie informacją o skonfigurowanym celu. trigger_invoked zawsze jest false; ta komenda nie uruchamia movera.

### Sterowanie bohaterem — opt-in

Ruch jest realizowany przez hook oCAIHuman::DoAI, a nie przez bezpośrednie przesuwanie pozycji ani własną symulację fizyki. Hook wstrzykuje wejście wyłącznie dla AI aktualnego, żywego bohatera w tym samym świecie. Tuż przed oryginalnym DoAI ustawia wybrany klawisz, zwiększa control_ai_frames, wywołuje oryginalne AI dokładnie raz, a następnie przywraca stan klawisza z tej klatki. Przy rozpoczęciu operacji zapisywany jest wcześniejszy stan wejścia; przy jej zakończeniu plugin go odtwarza, usuwa aktywną operację i zwraca input_released:true.

control_ai_frames potwierdza jedynie dotarcie wejścia do hooka. distance_moved to rzeczywista odległość euklidesowa 3D w surowych jednostkach świata Gothic. player_walk_to raz ustawia znormalizowany poziomy kierunek przez SetHeadingAtWorld, po czym emuluje wejście do przodu zwykłym przetwarzaniem AI. Nie używa pathfindingu, nie omija przeszkód i nie symuluje fizyki ani animacji.

Na punktach kontrolnych hooka renderującego najpierw sprawdzane jest anulowanie, a potem termin całego żądania; gdy oba warunki są widoczne jednocześnie, pierwszeństwo ma cancelled. Upływ duration_ms komendy player_move zwraca duration_elapsed nawet przy zerowym dystansie. player_walk_to zwraca target_reached po wejściu w tolerancję, movement_stalled przy braku postępu lub movement_timeout po upływie czasu przed dojściem. Upływ terminu żądania podczas sterowania również kończy operację jako movement_timeout. Anulowanie, wygaśnięcie, zmiana świata, utrata bohatera i błędy kończą operację przez przywrócenie wejścia i wyczyszczenie aktywnego sterowania.

Komendy player_teleport, player_turn, player_move i player_walk_to są domyślnie wyłączone. Wymagają allow_player_control:true w lokalnym pliku konfiguracji i potwierdzenia capabilities.mutating.player_control:true przez ping. Flaga player_control w capabilities.mutating jest zbiorczą informacją o dostępie, a nie nazwą komendy.

    {"command":"player_teleport","waypoint":"WP_TARGET","look_at":[0,0,0]}

player_teleport wymaga position albo waypoint; można dodać look_at. Zwraca pozycję przed, żądaną i rzeczywistą. Ustawienie pozycji używa SetPositionWorld; silnik ponownie sprawdza kolizję w następnym kroku ruchu. Teleport nie jest testem chodzenia.

    {"command":"player_turn","look_at":[100,0,200]}

player_turn zmienia poziomy kierunek bohatera. look_at musi różnić się poziomo od jego aktualnej pozycji.

    {"command":"player_move","direction":"forward","duration_ms":1000}

player_move przyjmuje direction: forward, backward, strafe_left albo strafe_right oraz duration_ms 100–30000 (domyślnie 1000). Wstrzykuje wejście gracza tuż przed przetworzeniem jego AI i po zakończeniu przywraca poprzedni stan klawisza. Odpowiedź podaje pozycję początkową i końcową, distance_moved, elapsed_ms, input_released, world_matched oraz control_ai_frames — liczbę klatek AI, w których plugin podał wejście. W scenie sterowanej skryptem lub przy blokadzie ruchu żądanie może zakończyć się bez przemieszczenia; sprawdzaj distance_moved zamiast zakładać ruch na podstawie reason.

W izolowanym testbedzie v0.2.0 (Gothic II 2.6 (fix), Union 1.0M, `useGothic1Controls=1`) test `strafe_left` przez 1 014 ms / 87 klatek AI dał 0,092 jednostki, a `strafe_right` przez 1 014 ms / 83 klatki dał 0,108 jednostki; w obu przypadkach współrzędne X/Z pozostały bez zmian, a wejście zostało zwolnione. Próby wykonano na otwartym, podpartym fragmencie `NW_NATURE_PATH_05`. Kod mapuje żądania na `GAME_STRAFELEFT` (18) i `GAME_STRAFERIGHT` (19), a testbed zawiera wpisy `keyStrafeLeft=d3001e00` i `keyStrafeRight=d1002000`.

Dla porównania, w tym samym profilu i punkcie użytkownik ręcznie przytrzymał zwykłe klawisze Strafe Left i Strafe Right. Po krótkim przytrzymaniu pozycja zmieniła się odpowiednio z `[31345.0,4566.6099,-19282.0]` na `[31695.9531,4579.3496,-19281.4141]` (X +350,9531) oraz z `[31345.0,4566.5381,-19282.0]` na `[30685.5508,4563.0791,-19280.9531]` (X −659,4492). Zatem świat i zwykłe wejście Gothica dopuszczały ruch boczny; nierozbieżność dotyczy wstrzykiwania wejścia pluginu, którego przyczyny nie wyizolowano. Traktuj strafe_left i strafe_right w `player_move` jako eksperymentalne. Raportuj pozycje i `distance_moved`; liczba klatek AI ani `reason=duration_elapsed` nie dowodzą ruchu. `player_walk_to` obraca postać i idzie do przodu, więc nie zastępuje testu strafe.

Wcześniejsza odpowiedź `player_move forward` zakończyła się w `[31554.6699,4557.1162,-19128.9570]`; późniejszy, osobny odczyt `player_state` zwrócił `[31538.0820,4564.9063,-19201.3184]`, czyli pozycję oddaloną o około 74,646 jednostki. Oba miejsca kodu korzystają z tej samej pozycji świata VOba, a odległość jest standardową normą euklidesową. Różnica jest niemal równoległa do poprzedniego kierunku ruchu (cosinus 0,99999) i odpowiada około 0,30 s wcześniejszej prędkości chodu. Jest zgodna z późniejszym odczytem świata po upływie czasu; ponieważ dokładnego odstępu i stanu wejścia nie zarejestrowano, nie można ustalić konkretnej przyczyny. Nie ma dowodu na błąd źródła pozycji lub obliczenia odległości.

    {"command":"player_walk_to","waypoint":"WP_TARGET","duration_ms":5000,"tolerance":100,"stagnation_ms":2500}

player_walk_to wymaga target jako wektora współrzędnych albo waypoint jako nazwy punktu. Przyjmuje duration_ms 100–30000 (domyślnie 5000), tolerance 20–1000 (domyślnie 100) i stagnation_ms 500–10000 (domyślnie 2500). Ustawia kierunek i przez czas operacji emuluje ruch do przodu. Nie wyznacza ścieżki omijającej przeszkody. Zwraca pozycje, dystans, target_distance, target_reached, przyczynę zakończenia, input_released i control_ai_frames. Zatrzymanie postępu kończy operację kodem movement_stalled. Przy przekroczeniu terminu, anulowaniu, zmianie świata albo utracie bohatera plugin zwalnia wejście.

### Screenshot i monitorowanie

screenshot nie wymaga dodatkowych pól. Po renderowaniu bieżącej klatki zapisuje plik TGA 32-bit w katalogu wymiany, pod generowaną, bezpieczną nazwą devbridge-<id>.tga. Nie nadpisuje istniejącego pliku. Odpowiedź zwraca filename, path, format, bytes i capture_stage. Plugin dostarcza obraz; jego interpretacja należy do klienta.

    {"command":"monitor","target_type":"player","duration_ms":5000,"interval_ms":250,"max_samples":20,"stagnation_ms":1000,"reference_vob":"NPC_VOB_NAME"}

target_type to player, npc albo mover; npc i mover wymagają target z nazwą VOba. Opcjonalny reference_vob dodaje reference_distance. duration_ms ma zakres 100–30000, interval_ms 50–5000, max_samples 1–256, a stagnation_ms 0–10000. Monitor zwraca próbki pozycji i zależnie od celu dane NPC, ruchu albo klatek movera. Kończy się po upływie czasu, limicie próbek albo wykryciu braku postępu. Żądanie nadal podlega deadline i plikowi cancel.<id>; po timeout/cancel operacja zwraca błąd i usuwa aktywne żądanie. Jednocześnie może działać tylko jedna operacja monitorowania lub sterowania bohaterem.

### Dodatkowe kody błędów

| Kod błędu | Znaczenie |
| --- | --- |
| vob_search_unavailable | Metadane klasy VOb nie są dostępne. |
| vob_not_found | Nie znaleziono wskazanego VOba lub punktu odniesienia. |
| vob_hierarchy_unavailable | Węzeł VOba w drzewie świata jest niedostępny albo uległ zmianie. |
| npc_search_unavailable | Metadane klasy NPC nie są dostępne. |
| npc_not_found | Nie znaleziono NPC. |
| object_not_npc | Wskazany VOb nie jest NPC. |
| player_unavailable | Bohater jest niedostępny, martwy albo nieświadomy. |
| waynet_unavailable | WayNet bieżącego świata nie jest dostępny. |
| waypoint_not_found | Nie znaleziono waypointa. |
| object_not_mover | Wskazany VOb nie jest zCMover. |
| input_unavailable | System wejścia gry nie jest dostępny dla sterowania bohaterem. |
| operation_busy | Inna operacja monitorowania lub sterowania jest aktywna. |
| target_not_found | Zniknął monitorowany cel. |
| reference_not_found | Zniknął obiekt odniesienia monitora. |
| player_changed | Aktywny obiekt bohatera zmienił się podczas monitorowania. |
| movement_stalled | Pozycja bohatera nie zrobiła wymaganego postępu; wejście zostało zwolnione. |
| movement_timeout | Próba dojścia nie dotarła do celu w wyznaczonym czasie. |
| movement_failed | Nieoczekiwany błąd operacji ruchu; wejście zostało zwolnione. |
| monitor_failed | Nieoczekiwany błąd monitorowania. |
| renderer_unavailable | Renderer nie jest dostępny podczas screenshotu. |
| screenshot_directory_unavailable | Nie można utworzyć katalogu wyjściowego screenshotu. |
| screenshot_exists | Wygenerowana ścieżka screenshotu już istnieje; plik nie jest nadpisywany. |
| screenshot_capture_failed | Nie udało się skopiować albo przygotować bufora klatki. |
| screenshot_save_failed | Nie udało się zapisać poprawnego, niepustego pliku TGA. |
| screenshot_failed | Nieoczekiwany błąd screenshotu. |
## Konfiguracja lokalna

Mapowanie scenario_dispatch.request_symbol musi wskazywać zapisywalną, skalarną globalną zmienną int. Każdy element status_symbols musi być skalarnym globalnym int; stała jest dopuszczalna dla odczytu statusu, lecz nie dla kolejki. Zmienne klasowe, tablice i typy inne niż int są odrzucane przed GetValue lub SetValue kodem configured_symbol_type_mismatch. Stała kolejka zwraca configured_symbol_not_writable. daedalus_read może odczytać stałe, ale nie odczytuje wartości zmiennych klasowych bez kontekstu instancji.

Utwórz `<game-root>\ZenGinDevBridge.config.json` tylko wtedy, gdy potrzebujesz zmienić domyślny katalog wymiany lub włączyć komendę zmieniającą stan. Plik nie może przekraczać 16 KiB i obsługuje następujący format:

    {
      "config_version": 1,
      "exchange_directory": "ZenGinDevBridge",
      "allow_camera_control": false,
      "allow_scenario_start": false,
      "allow_player_control": false,
      "scenario_dispatch": {
        "request_symbol": "BRIDGE_SCENARIO_REQUEST",
        "scenarios": {"example_step": 1},
        "status_symbols": {"queue_value": "BRIDGE_QUEUE_VALUE"}
      }
    }

`exchange_directory` musi być pojedynczą nazwą katalogu w katalogu głównym gry; ścieżki bezwzględne i przejścia do katalogu nadrzędnego są odrzucane. Zmiany kamery i scenariuszy są domyślnie wyłączone. Nazwy scenariuszy, identyfikatory i symbole Daedalusa przechowuj w tym lokalnym pliku; nie dodawaj mapowań zależnych od środowiska do repozytorium. Jeśli pliku nie można odczytać lub zweryfikować, możliwości zmieniające stan są wyłączane, a przyczyna jest raportowana przez `ping` i heartbeat.

Konfiguracja pluginu jest wczytywana i buforowana przy pierwszym uruchomieniu hooka renderującego po starcie gry. Zmiany pliku nie są stosowane w już działającym procesie — po każdej edycji zamknij grę i uruchom ją ponownie. Samo ponowne uruchomienie klienta PowerShell nie wystarczy.

Gdy `scenario_start` ma umieścić nowe żądanie w kolejce, plugin wyszukuje skonfigurowany symbol w parserze i przed odczytem lub zapisem sprawdza, czy jest skalarną zmienną całkowitoliczbową (`zPAR_TYPE_INT`, jeden element). Przy każdym odczycie przez `scenario_status` plugin sprawdza w ten sam sposób każdy skonfigurowany symbol statusu. Symbol kolejki `request_symbol` musi dodatkowo być zapisywalny, czyli nie może być stałą. Nieprawidłowy typ lub tablica zwraca `configured_symbol_type_mismatch`; stała kolejka zwraca `configured_symbol_not_writable`. `ping` potwierdza poprawność pliku konfiguracyjnego i obecność mapowań, ale nie potwierdza, że symbole istnieją lub mają poprawny typ w aktualnie załadowanym parserze.

## Zalecany sposób użycia przez agenta

1. Wywołaj `ping` i sprawdź `protocol_version`, `world_ready` oraz `capabilities`.
2. Do obserwacji używaj `vob`, `raycast`, `raycast_batch` lub `surface_grid`.
3. Przed camera_set, scenario_start lub poleceniem player_* sprawdź, czy odpowiednia capability ma wartość true, oraz uzyskaj jawną zgodę użytkownika lub systemu wywołującego.
4. Używaj unikalnego identyfikatora żądania, ustaw termin ważności i czekaj na odpowiedź z tym samym `id`.
5. Po przekroczeniu limitu czasu przez klienta zapisz `cancel.<id>` i zaczekaj na odpowiedź potwierdzającą anulowanie przed wysłaniem kolejnego żądania.
6. Po zmianie kamery wywołaj `camera_restore` i sprawdź wynik w polu `restored`.

Aby ustawić aktualny termin, w chwili tworzenia żądania oblicz czas Unix w milisekundach i dodaj do niego wybrany limit czasu. Nie używaj ponownie przykładowego terminu z tej dokumentacji.
