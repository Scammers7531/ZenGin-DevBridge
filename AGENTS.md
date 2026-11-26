# Development guidance

## Obowiązkowa polityka językowa / Mandatory language policy

**Językiem podstawowym projektu jest polski, a dodatkowym angielski. Zasady poniżej są obowiązkowe dla każdego agenta AI pracującego w tym repozytorium.**

- Każdy nowy dokument publiczny napisz najpierw po polsku, a następnie przygotuj jego pełne tłumaczenie na angielski.
- Utrzymuj `README.md` po polsku jako domyślną stronę projektu na GitHubie, a `README.en.md` po angielsku.
- Dokumentację API utrzymuj w `docs/API.md` po polsku i `docs/API.en.md` po angielsku. Instrukcje, poradniki, przykłady oraz pozostałą dokumentację publiczną prowadź w analogicznych parach językowych, w tym `THIRD_PARTY_NOTICES.md` po polsku i `THIRD_PARTY_NOTICES.en.md` po angielsku.
- Wydania, changelog i opisy nowych funkcji prezentuj najpierw po polsku, a następnie po angielsku. Pliki changeloga to `CHANGELOG.md` i `CHANGELOG.en.md`; opis GitHub Release również ma mieć kolejność PL, potem EN.
- Każda zmiana funkcjonalności wymaga równoczesnej aktualizacji obu wersji językowych w tym samym zestawie zmian. Nie publikuj niepełnego tłumaczenia ani wersji, które różnią się zakresem lub znaczeniem.
- Nazwy funkcji, klas, komend API, parametrów i pól JSON pozostawiaj po angielsku. Przykłady kodu w parach dokumentów muszą być identyczne; tłumacz wyłącznie otaczający je opis.
- Na początku dokumentów dwujęzycznych zachowuj widoczny przełącznik `🇵🇱 Polski | 🇬🇧 English`, z polskim jako pierwszym wyborem.
- Przed commitem sprawdź kompletność obu wersji, zgodność merytoryczną, linki językowe i niezmienność nazw oraz przykładów kodu.
- Teksty licencji i wymagane noty prawne stron trzecich zachowuj dokładnie w oryginalnym brzmieniu; nie zastępuj ich tłumaczeniem.

**English translation — the same rules are mandatory for every AI agent working in this repository.**

- Polish is the project's primary language; English is the secondary language. Write every new public document in Polish first, then produce a complete English translation.
- Keep `README.md` in Polish as GitHub's default project page and `README.en.md` in English.
- Keep the API reference in `docs/API.md` in Polish and `docs/API.en.md` in English. Maintain installation instructions, guides, examples, and all other public documentation in matching language pairs, including `THIRD_PARTY_NOTICES.md` in Polish and `THIRD_PARTY_NOTICES.en.md` in English.
- Present release information, the changelog, and new feature descriptions in Polish first and English second. The changelog files are `CHANGELOG.md` and `CHANGELOG.en.md`; GitHub Release descriptions must use the same PL-then-EN order.
- Every functional change requires both language versions to be updated in the same change set. Do not publish incomplete translations or versions that differ in scope or meaning.
- Keep function, class, API command, parameter, and JSON field names in English. Code examples in each documentation pair must be identical; translate only the surrounding prose.
- Preserve a visible `🇵🇱 Polski | 🇬🇧 English` language switch at the top of bilingual documents, with Polish as the first choice.
- Before committing, verify both versions are complete and semantically aligned, links work, and code names and examples are unchanged.
- Preserve license texts and required third-party legal notices verbatim; do not replace them with translations.

## Canonical source and deployment

- This repository is the canonical source project for ZenGin DevBridge.
- Maintain one DevBridge implementation. Keep environment-specific behavior in local configuration rather than in a consumer-specific fork.
- Develop universal features and fixes here first. Verify them in an isolated runtime before planning a separate transfer to a consuming installation.
- Never automatically copy, overwrite, or deploy builds to a consuming or production installation. A deployment must be a separate deliberate step with a backup and a record of source revision, plugin version, and deployed DLL SHA-256.

## Compatibility and project boundaries

- Preserve compatibility with existing test clients: keep established commands, request payloads, response fields, and the one-request-at-a-time file workflow when possible.
- Keep protocol additions backward-compatible. Requests without `protocol_version` are treated as protocol version 1; version additions and any breaking changes must be documented in both `docs/API.md` and `docs/API.en.md`, and both `CHANGELOG.md` and `CHANGELOG.en.md`.
- Use local configuration for environment-specific exchange-directory names, scenario names, and Daedalus symbol mappings. Do not hard-code private scenario names or symbols into the public core.
- Do not include private scripts, story content, game assets, logs, local audit reports, personal data, toolchains, compiler files, or Windows SDK files in public commits or release packages.
- Keep new features within the approved ZenGin/Gothic II scope; do not add MCP or support for other Gothic versions in this release line.

## Engineering and release

- Keep changes narrow and preserve working universal diagnostics, raycasts, `surface_grid`, and camera tools.
- Treat camera changes and scenario dispatch as state-changing operations. Keep them disabled by default, expose their availability through capabilities, and retain a safe restore path.
- Keep the file protocol local. Validate request size, IDs, numeric inputs, deadlines, and world identity before using game pointers.
- Keep third-party source pinned to reviewed commits and preserve each dependency's required notices in both `THIRD_PARTY_NOTICES.md` and `THIRD_PARTY_NOTICES.en.md`, and in binary release materials.
- Record supported game/Union versions, protocol changes, release version, and the built DLL SHA-256 in project documentation.
- Build and test in an isolated game copy when runtime validation is needed. Do not alter original consumer or production installations while developing this canonical project.
- Keep work local until the maintainer explicitly approves publishing. Never push this repository to GitHub without that approval.

## Zarządzanie procesami testowymi / Test process management

- Nie pozostawiaj uruchomionego Gothica ani innych zbędnych programów po zakończeniu testów.
- Rejestruj PID i pełną ścieżkę każdego procesu uruchomionego na potrzeby testbedu.
- Zamykaj tylko zweryfikowane procesy uruchomione przez agenta; nigdy nie kończ wszystkich procesów o danej nazwie.
- Nie zamykaj programów użytkownika ani procesów innych instalacji Gothica. Nie ingeruj w GO3 ani GO3 DEV.
- Preferuj prawidłowe zamknięcie. Zakończenie procesu wymuś tylko po sprawdzeniu PID i ścieżki oraz po nieskutecznej próbie normalnego zamknięcia.
- Kontroluj procesy po każdym etapie testowym, przerwaniu i niepowodzeniu. W raporcie końcowym potwierdź, czy pozostał aktywny którykolwiek proces uruchomiony przez agenta.
- Prowadź lokalny dziennik postępu ułatwiający wznowienie pracy; nie dodawaj do publicznego repozytorium prywatnych raportów ani danych środowiska testowego.

- Do not leave Gothic or other unnecessary programs running after tests.
- Record the PID and full path of every process started for the testbed.
- Close only verified processes started by the agent; never terminate every process with a matching name.
- Do not close the user's programs or processes from other Gothic installations. Do not interfere with GO3 or GO3 DEV.
- Prefer a normal shutdown. Force-terminate only after checking the PID and path and after a normal shutdown has failed.
- Check processes after each test stage, interruption, and failure. In the final report, confirm whether any process started by the agent remains active.
- Keep a local progress log so work can resume; do not add private reports or testbed data to the public repository.
