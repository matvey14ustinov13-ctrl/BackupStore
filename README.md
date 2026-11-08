# BackupStore

Магазин приложений Backup Team для Windows. Написан на C++ (Win32), без .NET и Python. Каталог и релизы берутся с GitHub, программы скачиваются и распаковываются локально.

## Возможности

- Каталог приложений Backup Team с версиями, описаниями и лицензиями.
- Установка из ZIP-архивов и обычных `.exe` из GitHub Releases.
- Запуск установленных программ из магазина и открытие папки установки.
- Проверка обновлений установленных приложений и самого BackupStore.
- Самообновление через GitHub Releases.
- Тёмный интерфейс, поиск по каталогу, разделы «Установленные» и «Обновления».
- Кэш каталога в `catalog.json`, который автоматически обновляет GitHub Actions.

## Установка

Скачайте `BackupStore.exe` из последнего релиза и запустите. Установка не требуется: программа хранит данные в `%LOCALAPPDATA%\BackupStore`.

- `apps` — установленные программы;
- `downloads` — скачанные архивы;
- `cache` — кэш каталога;
- `installed.json` — список установленных приложений.

## Сборка

Нужны Visual Studio с C++-инструментами и CMake 3.20+.

```bat
build.bat
```

Готовый файл: `build\Release\BackupStore.exe`.

## Каталог

`catalog.json` собирается скриптом `scripts/build_catalog.py` и обновляется workflow `.github/workflows/catalog.yml` каждые 6 часов. В каталог попадают публичные репозитории Backup Team, у которых есть релиз. Если файл каталога недоступен, BackupStore запрашивает GitHub API напрямую.

## Лицензия

Проект распространяется под лицензией [GPL v3](https://www.gnu.org/licenses/gpl-3.0.html). Полный текст — в файле [LICENSE](LICENSE).

Сторонние компоненты:

- [nlohmann/json](https://github.com/nlohmann/json) — MIT;
- [miniz](https://github.com/richgel999/miniz) — public domain.

© 2026 Backup Team
