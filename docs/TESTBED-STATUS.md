[🇵🇱 Polski](TESTBED-STATUS.md) | [🇬🇧 English](TESTBED-STATUS.en.md)

# Odizolowane środowisko testowe

Daty weryfikacji: 2026-11-26–27

## Weryfikacja wydania v0.2.0 (2026-11-27)

Gothic II 2.6 (fix), x86, Union 1.0M, protokół 1. Finalna DLL Windows x86: SHA-256 `B28E10A4441DA8DE7687C60AF2E99B27B1B7F3B0A8F21E063A737032216D4CC4`.

- 11/11 kontroli runtime i 7/7 kontroli zgodności wstecznej na finalnej DLL.
- Potwierdzono ruch do przodu (178,11 jednostki / 713 ms), dojście do dwóch celów i zatrzymanie na stromym stoku; anulowanie oraz limit czasu zwalniały wejście. Nie potwierdzono ruchu bocznego przez plugin mimo działającego ręcznego sterowania.
- Odczytano globalne symbole int, float, string i tablice; zmienne klasowe jako kolejka/status zostały odrzucone. Potwierdzono obrót, teleport, odczyt movera, próbkowanie NPC/movera i obraz TGA 800×600×32. Nie potwierdzono niepustej hierarchii VObów ani zmiany pozycji poruszającego się movera.
- Test ruchu wymaga sterowalnego świata poza dialogami. `dialogue.active` może nie wykryć widocznego dialogu. Przyczyna dodatkowego przesunięcia 74,646 jednostki między odczytami nie została rozstrzygnięta.
- Nie testowano zmiany świata podczas aktywnego ruchu ani konfliktów z innymi pluginami. Brak porównywalnego benchmarku finalnej DLL; dane pomiarowe wcześniejszej kompilacji opisano w [dzienniku zmian](../CHANGELOG.md).
- Testbed i zapisy pozostają lokalne; po testach przywrócono konfigurację i zamknięto procesy testowe.

Zobacz [publiczne wydanie v0.2.0](https://github.com/Scammers7531/ZenGin-DevBridge/releases/tag/v0.2.0).

## Wyniki historyczne (v0.1.1 i v0.1.0)

## Weryfikacja kandydata 0.1.1

Test wykonano w rzeczywiście uruchomionym Gothic II 2.6 (fix) z Union 1.0M. DLL kandydata skopiowano wyłącznie do `build/testbed/System/Autorun`; poprzednią bibliotekę zachowano w lokalnym katalogu `_rollback`. Plugin zgłosił `plugin_version: 0.1.1`, `protocol_version: 1` i `world_ready: true`.

Testy regresji zakończyły się powodzeniem dla `ping`, `vob` na `PC_HERO`, `raycast`, `camera_get`, `camera_restore` bez aktywnego nadpisania oraz `surface_grid` z siatką 3×3 (9 wyników). Przywrócono domyślną konfigurację testbedu, w której `allow_camera_control` i `allow_scenario_start` pozostają wyłączone.

Weryfikacja symboli Daedalusa użyła istniejących symboli gry oraz jednego testowego symbolu dodanego wyłącznie w testbedzie. Skalarna stała całkowitoliczbowa `TRUE` została poprawnie odczytana przez `scenario_status` (wartość 1). Użycie `TRUE` jako kolejki zakończyło się `configured_symbol_not_writable`. Symbol instancji `PC_HERO` został odrzucony zarówno dla statusu, jak i kolejki kodem `configured_symbol_type_mismatch`, przed odczytem lub zapisem wartości. Testowy symbol `DEVBRIDGE_TEST_REQUEST` zwrócił początkowo 0, po `scenario_start` przyjął wartość 123, a `scenario_status` potwierdził ten zapis. Jednowierszowy fixture znajduje się wyłącznie w `build/testbed/System/Autorun/DevBridgeTestSymbols.d`; nie jest częścią repozytorium ani paczki wydania.

SHA-256 przetestowanej DLL Windows x86:

`C4E8213AE394352550FF31709A83E6FEA8FC6118CCDA7435554BE47AB552644A`

## Zakres lokalny

Kopia uruchomieniowa Gothic II znajduje się w `build/testbed` i jest wykluczona z repozytorium regułą `/build/` w katalogu głównym. Pliki gry pozostają lokalnie i nie są częścią publicznych źródeł ani paczki wydania.

## Zweryfikowane wersje

- Gothic II: 2.6 (fix)
- Union: 1.0M
- ZenGin DevBridge: 0.1.0
- Protokół DevBridge: 1

SHA-256 skompilowanego pluginu:

`DF60FBF41E927CFFC3F791B50979D64C5743DFD3E9D789FE16BD54EDE3CB9E3A`

## Testy działania

Uruchomienie bazowe bez DevBridge oraz uruchomienie z pluginem dotarły do sceny początkowej Xardasa; muzyka i dźwięk działały. Po załadowaniu DevBridge z `System/Autorun` plugin zgłosił `world_ready: true`.

Polecenie `ping` klienta PowerShell zwróciło `ok: true`, wersję protokołu 1 i wersję pluginu 0.1.0. Testy działania przeszły dla `ping`, `vob` na `PC_HERO`, `raycast`, `raycast_batch` z dwiema próbkami oraz pełnego `surface_grid` ze 121 próbkami. Sprawdzone próbki siatki zwróciły poprawne wyniki i dane materiału podłoża świata.

Przy domyślnej konfiguracji lokalnej `camera_set` i `scenario_start` zwróciły `capability_disabled`, a `camera_get` zadziałało. `camera_restore` bez aktywnego nadpisania zakończyło się bezpiecznie i bez awarii. W osobnym teście wyłącznie w testbedzie, po jawnym włączeniu sterowania kamerą, `camera_set` ustawiło nadpisanie, a `camera_restore` przywróciło zapisaną transformację i flagi AI kamery w tym samym świecie. Następnie konfigurację testbedu ponownie ustawiono tak, by obie funkcje zmieniające stan były wyłączone.

Test symulowanego protokołu przeszedł dla obsługi wersji i terminu żądania, zgodności identyfikatorów odpowiedzi, anulowania po przekroczeniu limitu czasu i potwierdzenia anulowania. Podczas tych testów uruchomienia i komend nie wystąpiła awaria. Zmianę świata w trakcie aktywnej operacji sprawdzono w kodzie, ale nie wymuszano jej w teście działania; nie jest to test długotrwałej stabilności.

## Krótki pomiar bezczynności

W tej samej scenie początkowej wykonano dwie próbki trwające 15,02 s: bez DLL DevBridge i z załadowanym pluginem. Czas CPU procesu wyniósł 15,234 s bez pluginu (101,43% jednego procesora logicznego) oraz 15,188 s z pluginem (101,08%). Różnica 0,35 punktu procentowego mieści się w szumie krótkiego pomiaru. W tym teście nie zmierzono dodatkowego obciążenia CPU w stanie bezczynności. Pomiar nie obejmuje kosztu aktywnych skanów, innych scen ani długotrwałego działania.

Lokalna kopia uruchomieniowa zajmowała 2 840 313 904 bajty (2,840314 GB / 2,645248 GiB) podczas ostatniego pomiaru. To rozmiar lokalny i może się zmienić wraz ze zmianami instalacji gry.

Podczas weryfikacji nie zmieniano żadnej instalacji użytkowej ani produkcyjnej i nie wdrażano do nich kompilacji.
