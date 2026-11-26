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

Konfiguracja pluginu jest wczytywana i buforowana przy pierwszym uruchomieniu hooka renderującego po starcie gry. Zmiany pliku nie są stosowane w już działającym procesie — po każdej edycji zamknij grę i uruchom ją ponownie. Samo ponowne uruchomienie klienta PowerShell nie wystarczy.

Gdy `scenario_start` ma umieścić nowe żądanie w kolejce, plugin wyszukuje skonfigurowany symbol w parserze i przed odczytem lub zapisem sprawdza, czy jest skalarną zmienną całkowitoliczbową (`zPAR_TYPE_INT`, jeden element). Przy każdym odczycie przez `scenario_status` plugin sprawdza w ten sam sposób każdy skonfigurowany symbol statusu. Symbol kolejki `request_symbol` musi dodatkowo być zapisywalny, czyli nie może być stałą. Nieprawidłowy typ lub tablica zwraca `configured_symbol_type_mismatch`; stała kolejka zwraca `configured_symbol_not_writable`. `ping` potwierdza poprawność pliku konfiguracyjnego i obecność mapowań, ale nie potwierdza, że symbole istnieją lub mają poprawny typ w aktualnie załadowanym parserze.

## Zalecany sposób użycia przez agenta

1. Wywołaj `ping` i sprawdź `protocol_version`, `world_ready` oraz `capabilities`.
2. Do obserwacji używaj `vob`, `raycast`, `raycast_batch` lub `surface_grid`.
3. Przed `camera_set` lub `scenario_start` sprawdź, czy dana możliwość zmieniająca stan ma wartość `true`, oraz uzyskaj jawną zgodę użytkownika lub systemu wywołującego.
4. Używaj unikalnego identyfikatora żądania, ustaw termin ważności i czekaj na odpowiedź z tym samym `id`.
5. Po przekroczeniu limitu czasu przez klienta zapisz `cancel.<id>` i zaczekaj na odpowiedź potwierdzającą anulowanie przed wysłaniem kolejnego żądania.
6. Po zmianie kamery wywołaj `camera_restore` i sprawdź wynik w polu `restored`.

Aby ustawić aktualny termin, w chwili tworzenia żądania oblicz czas Unix w milisekundach i dodaj do niego wybrany limit czasu. Nie używaj ponownie przykładowego terminu z tej dokumentacji.
