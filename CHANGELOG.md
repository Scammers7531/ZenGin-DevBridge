[🇵🇱 Polski](CHANGELOG.md) | [🇬🇧 English](CHANGELOG.en.md)

# Dziennik zmian

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
