[🇵🇱 Polski](TESTBED-STATUS.md) | [🇬🇧 English](TESTBED-STATUS.en.md)

# Odizolowane środowisko testowe

Data weryfikacji: 2026-11-26

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
