# TH01 — открытый порт на Nintendo Switch (экспериментальный)

> **Статус 2026-09-08:** исходный порт собирается и проходит тесты на Linux; NRO для Switch
> собирается в CI (devkitPro/devkitA64 + libnx), иконка и NACP проверяются утилитой
> `scripts/verify_th01_nro.py`. **На реальной консоли не запускался.** Это не «полный порт игры»
> и не обещание отсутствия ошибок. Оригинальные данные игры не включены и не загружаются.

![Platform](https://img.shields.io/badge/Platform-Nintendo_Switch-e60012?style=for-the-badge)
![Distribution](https://img.shields.io/badge/Distribution-Source_Only-238636?style=for-the-badge)
![Data](https://img.shields.io/badge/Game_Data-Bring_Your_Own-218bff?style=for-the-badge)

**English summary.** An original, source-only homebrew that reimplements the *gameplay shape* of
the first PC-98 Touhou title: card-breaking stages alternating with boss duels. It is written from
scratch in C, renders procedurally, ships no game data, and reads an optional open content package
(`sdmc:/switch/th01/th01open.dat`) that you build from your own content. It uses only open tooling
(devkitA64 + libnx); no Nintendo SDK and no proprietary runtime.

---

## Что это

- **Оригинальная реализация**, написанная с нуля: симуляция с фиксированным шагом кадра и
  целочисленной математикой (результат одинаков на ПК и на Switch — это проверяется тестом
  `determinism`).
- **Свой процедурный контент**: 8 этапов (карточные поля + три босса) генерируются кодом,
  графика рисуется примитивами, музыка — маленьким синтезатором. Ни одного чужого спрайта,
  сэмпла или шрифта в репозитории.
- **Два бэкенда**: host (рендер кадров в PPM + WAV на диске) и switch (libnx: фреймбуфер, геймпад,
  audout). Есть и headless-режим для тестов.
- **BYOD-данные**: при наличии `sdmc:/switch/th01/th01open.dat` порт играет ваш набор этапов
  (см. [формат](docs/TH01_DATA_FORMAT.md)); нет файла — играет встроенный набор.

## Чем это не является

- Это **не** эмулятор PC-98 и **не** порт оригинального кода: исходники игры здесь не используются
  и не декомпилируются.
- Это **не** дистрибутив игры: в репозитории нет данных, графики, музыки или торговых марок
  правообладателей. Тест `test_no_game_data_is_committed` следит, чтобы бинарники не попали в Git.
- Здесь **нет** Nintendo SDK: собирается обычным homebrew-тулчейном devkitA64 + libnx (открытый).
  Для запуска нужна консоль с атмосферой/homebrew-загрузчиком — сама по себе сборка этого не даёт и
  не содержит прошивок, ключей и проприетарных рантаймов.

## Право

Графика, музыка, персонажи и торговые марки принадлежат их правообладателям. Мы не
распространяем оригинальные файлы, не обходим DRM и не даём лицензий на чужие данные.
Весь код порта — оригинальный, MIT ([LICENSE.md](LICENSE.md)); шрифт нарисован для проекта
(`scripts/gen_th01_font.py`), иконка NRO — процедурная.

## Геймплей и управление

| Кнопка | Действие |
| --- | --- |
| D-Pad / левый стик | движение влево-вправо |
| A (или ZR) | карточный этап: запуск орба / взмах жезлом; этап босса: стрельба |
| B (или ZL) | бомба (только на этапах босса) |
| X / Y | подтверждение |
| Plus | пауза |
| Minus | выход (с сохранением) |

Карточный этап: орб отскакивает от стен, карточек и жезла; карты с двумя HP, «каменные» и
«силовые» (дают уровень выстрела). Потеря орба — минус жизнь. Этап босса: перестрелка с эмблемой с
тремя типами паттернов, полоска HP, бомба чистит пули.

## Сборка

```bash
# тесты ядра (C), без тулчейна
python3 scripts/test_th01_native.py --sanitize

# симуляция: сценарии boot/card/boss/death/determinism
python3 scripts/build_th01.py --target headless --frames 1800

# host-сборка: кадры в dist/th01/host/frames, WAV и preview-GIF
python3 scripts/build_th01.py --target host --frames 1200 --every 5

# Switch NRO: нужен devkitPro/devkitA64 + libnx (в CI — контейнер devkitpro/devkita64)
python3 scripts/build_th01.py --target switch
```

Результат Switch-сборки: `dist/th01/switch/th01.nro`, `TH01-experimental-switch.zip`,
`th01-source.tar.gz` (ровно те исходники, из которых собран NRO), `th01-symbols.zip` и `BUILD.json`.

## Установка на Switch

См. [TH01_INSTALL.txt](TH01_INSTALL.txt): скопировать `th01.nro` в `sdmc:/switch/th01/`,
при желании положить туда же свой `th01open.dat`. Сейв (рекорд, счётчик прохождений) пишется
в `sdmc:/switch/th01/th01.sav`, диагностика — в `th01.log`.

## Данные (BYOD)

Подробно — в [docs/TH01_DATA_FORMAT.md](docs/TH01_DATA_FORMAT.md) (формат контейнера) и
[docs/TH01_ORIGINAL_DATA.md](docs/TH01_ORIGINAL_DATA.md) (что можно и чего нельзя легально
делать с вашей копией).

```bash
python3 scripts/pack_th01_data.py --emit-example my-content   # шаблон
# …правим my-content/content.json…
python3 scripts/pack_th01_data.py --source my-content --output th01open.dat
python3 scripts/pack_th01_data.py --verify th01open.dat
```

## Тесты

```bash
python3 -m unittest discover -s tests -v      # публичные тесты, stdlib только
python3 scripts/test_th01_native.py --sanitize
```

Что проверяется: формат пакета (сборка + независимый разбор + отклонение битого входа),
детерминизм симуляции, физика орба, урон по карточкам и боссу, потеря жизни, пауза,
ротация сейва (tmp → rename + одна резервная копия), политика кадров при выходе,
иконка NRO и разбор настоящего NRO, и отсутствие чужих данных в дереве.

## Известные ограничения

1. **На железе не проверялось.** Есть риск геймпадных, звуковых и тайминговых сюрпризов.
2. Встроенный набор — демо: 8 этапов, процедурная графика, синтезатор вместо музыки.
3. Загрузка оригинальных файлов PC-98 **не реализована**: форматы не документированы
   публично, а у проекта нет ни файлов, ни разрешения на их распаковку. Для разведки есть
   `scripts/probe_th01_original.py` (только локальный осмотр ваших файлов).
4. Сеть не используется ни портом, ни сборкой: ничего не скачивается.
