[🇵🇱 Polski](THIRD_PARTY_NOTICES.md) | [🇬🇧 English](THIRD_PARTY_NOTICES.en.md)

# Informacje o kodzie stron trzecich

Biblioteka DLL DevBridge statycznie łączy Union API i włączone biblioteki pomocnicze. Teksty praw autorskich i licencji tych komponentów znajdują się w katalogu `LICENSES/`. Podmoduły źródłowe są przypięte do rewizji wymienionych poniżej.

| Komponent | Wersja / rewizja | Licencja | Uwagi |
| --- | --- | --- | --- |
| Union API | `167c5573653a8d89c1fee01865b1ac4502935e3f` | BSD 3-Clause | Statyczna implementacja API i obsługa hooków. |
| Gothic API | `6e554b0acb199c9544037fb7136e07fad44a2fc4` | BSD 3-Clause | Nagłówki API Gothic II: Noc Kruka i kod obsługi wersji. |
| Microsoft Detours | 4.0.1, `e4bfd6b03e50de46b47abfbd1e46b384f0c5f833` | MIT | Dołączone przez Union API. |
| libogg | 1.3.5, `e1774cd77f471443541596e09078e78fdc342e4f` | Licencja Xiph w stylu BSD | Dołączona przez Union API. |
| libvorbis | `bb4047de4c05712bf1fd49b9584c360b8e4e0adf` | Licencja Xiph w stylu BSD | Dołączona przez Union API. |
| zlib | 1.3.1, `51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf` | Licencja zlib | Dołączona przez Union API. |
| nlohmann/json | 3.11.3, pojedynczy plik nagłówkowy | MIT | `src/json.hpp`; identyfikatory praw autorskich i licencji SPDX pozostają w nagłówku. |

Katalog `LICENSES/` zawiera treści licencji wymagane w dystrybucjach źródłowych i binarnych. Plik binarny importuje również systemowe biblioteki Windows `KERNEL32.dll` i `dbghelp.dll`; nie są one dołączane do DevBridge.

## Weryfikacja szablonu pluginu Union

Sprawdzone publiczne repozytorium szablonu pluginu Union autorstwa Patrix9999 nie publikuje pliku `LICENSE` w analizowanej gałęzi domyślnej. DevBridge nie rozpowszechnia szkieletu projektu, plików kompilacji, kodu punktu wejścia ani zasobów tego szablonu. Projekt korzysta z własnego systemu kompilacji CMake i własnej nakładki dla Gothic II: Noc Kruka. Szablon nie znajduje się w podmodułach źródłowych ani w paczce wydania.

Nazwy licencji oraz ich wymagane teksty w katalogu `LICENSES/` są zachowane w oryginalnym brzmieniu; niniejszy opis ich nie zastępuje.
