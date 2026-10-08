# Распознавание автомобильных номеров РФ

C++17, OpenCV 4.x/5.x, Tesseract LSTM. Вход — папка изображений
JPG/JPEG/PNG/BMP (без рекурсии), выход — одна строка на каждый файл:

```csv
filename,plate_number,confidence,time_ms
```

Номер записывается латинскими визуальными эквивалентами кириллицы, например
`A123BC777`. Если валидного результата нет, `plate_number` пустой,
`confidence=0`. Confidence — исходный `MeanTextConf` Tesseract (0–100),
**не вероятность правильного распознавания**. `time_ms` включает чтение,
детекцию, OCR и сохранение отладки, но не инициализацию моделей и запись CSV.
Имена файлов с запятыми/кавычками экранируются.

## Linux (Ubuntu/Debian, системный OpenCV 4)

```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build pkg-config python3 \
  libopencv-dev libtesseract-dev libleptonica-dev tesseract-ocr-eng
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/plate_recognizer test_images data/haarcascade_russian_plate_number.xml output/results.csv --debug output/debug
./build/evaluate output/results.csv ground_truth.csv output/errors.csv
```

## Windows: MSYS2 UCRT64

Откройте именно **MSYS2 UCRT64**, обновите систему (`pacman -Syu`, при
запросе перезапустите терминал и повторите). Не смешивайте библиотеки UCRT64,
MINGW64 и системный MSYS `/usr/bin/g++`.

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-opencv \
  mingw-w64-ucrt-x86_64-tesseract-ocr \
  mingw-w64-ucrt-x86_64-tesseract-data-eng \
  mingw-w64-ucrt-x86_64-leptonica \
  mingw-w64-ucrt-x86_64-python

cd /c/path/to/plate_recognition
bash scripts/build_msys2.sh
./build-ucrt64/plate_recognizer.exe test_images data/haarcascade_russian_plate_number.xml output/results.csv --debug output/debug --tessdata C:/msys64/ucrt64/share/tessdata
./build-ucrt64/evaluate.exe output/results.csv ground_truth.csv output/errors.csv
```

Замените путь `C:/msys64` на свой. Из PowerShell/CMD добавьте
`C:\msys64\ucrt64\bin` в PATH: без него Windows не найдёт DLL OpenCV,
Tesseract и Leptonica. Для путей с пробелами нужны кавычки. Для первого
запуска предпочтительны ASCII-пути: Unicode-пути Windows через `imread`
зависят от сборки OpenCV. В VS Code открывайте проект из UCRT64 (`code .`)
и используйте CMake-задачи; сборка одного активного `.cpp` через gcc не подходит.

## OpenCV 4 и 5, необязательный каскад

Обязательные компоненты: `core`, `imgproc`, `imgcodecs`; для OpenCV 5
также `geometry`. CMake сначала определяет версию, затем запрашивает
нужные компоненты. Каскад — опциональный `objdetect` в 4.x или `xobjdetect`
из opencv_contrib в 5.x. При отсутствии модуля код с `CascadeClassifier`
не компилируется и не линкуется; контурный метод остаётся доступен.
Заголовки разделены в `src/opencv_compat.hpp` через `CV_VERSION_MAJOR >= 5`.

Чтобы выбрать отдельную установку OpenCV, передайте
`-DOpenCV_DIR=/path/to/opencv/lib/cmake/opencv4` (фактическая папка,
содержащая `OpenCVConfig.cmake`). Не используйте старый каталог сборки
после смены версии OpenCV или MSYS2 toolchain.

Проверка сборки вообще без каскада:

```bash
cmake -S . -B build-contours -G Ninja -DPLATE_ENABLE_CASCADE=OFF
cmake --build build-contours --parallel
ctest --test-dir build-contours --output-on-failure
./build-contours/plate_recognizer test_images - output/contours.csv
```

`-` вместо XML отключает каскад во время запуска. Отсутствующий или
повреждённый XML тоже приводит к предупреждению и контурному режиму.
`geometry` в OpenCV 5 является обязательным: его отсутствие вызывает
понятную ошибку конфигурации, а не ошибки компилятора в исходниках.

## Запуск и отладка

```text
plate_recognizer INPUT_DIR [CASCADE.xml|-] [OUTPUT.csv]
                 [--debug DIR] [--candidates N] [--tessdata DIR]
```

По умолчанию: XML из `data/`, `output/results.csv`, до 6 кандидатов.
Параметр `--candidates` принимает 1–100. Прежние позиционные аргументы
сохранены. Пути считаются от текущей рабочей папки.

В `--debug DIR` для каждого кадра создаётся папка с индексом и полным
именем файла. В ней:

- `boxes.png` — все найденные боксы с индексами;
- `candidate_N_crop.png` — исходная вырезка;
- `candidate_N_warp.png` — результат выравнивания (либо вырезка с отступами);
- `candidate_N_otsu.png`, `candidate_N_adaptive.png` — входы OCR;
- `candidate_N_ocr.txt` — исходный текст, исправления, валидность,
  confidence и оценка каждого варианта бинаризации.

Без `--debug` промежуточные файлы не создаются. Код выхода распознавателя:
0 — файлы обработаны (возможны нераспознанные номера), 1 — ошибка запуска,
2 — ошибка хотя бы одного кадра. Для такого кадра сохраняется пустой
результат, остальные продолжают обрабатываться. Ошибка инициализации OCR
останавливает программу; молчаливой работы с неинициализированным Tesseract нет.

## Пайплайн и правила OCR

1. Grayscale → Haar, если доступен, и контуры Canny/morphology.
   Контуры рассматриваются и при срабатывании Haar. Повторяющиеся боксы
   удаляются по IoU > 0,65; геометрия оценивается по повёрнутому прямоугольнику.
2. В окрестности бокса ищутся выпуклые четырёхугольники `approxPolyDP`.
   Учитываются площадь и пропорции; упорядоченные четыре угла передаются
   в `getPerspectiveTransform`/`warpPerspective`. Если надёжных углов нет,
   используется исходная вырезка с отступом.
3. Масштабирование к высоте около 96 px (масштаб ограничен 0,25–4), Otsu
   и adaptive threshold, белая рамка, `PSM_SINGLE_LINE`, язык `eng`.
4. Результат очищается от пробелов/дефисов, приводится к верхнему регистру,
   кириллические визуальные эквиваленты переводятся в латиницу. Непонятные
   символы не удаляются, длина строки не подгоняется и подстроки не вырезаются.
5. Формат: `[ABEKMHOPCTYX][0-9]{3}[ABEKMHOPCTYX]{2}[0-9]{2,3}`.
   На буквенных позициях: `0→O`, `8→B`, `4→A`.
   На цифровых: `O/Q/D→0`, `B→8`, `A→4`, `I/L→1`, `S→5`, `Z→2`, `T→7`.
   Это эвристики; они не гарантируют правильность. Код региона проверяется
   только по длине/цифрам, а не по актуальному реестру выдачи.
6. Валидный формат имеет приоритет. Между валидными вариантами сравнивается
   `max(0, confidence - 7 × число исправлений)`, затем число исправлений.
   Это внутренняя оценка выбора, в CSV остаётся исходный confidence.
   Вариант `A125BC777` уже валиден — заменить 5 на 3 без дополнительных
   свидетельств нельзя. Если все варианты невалидны, результат пустой.

## Оценка качества

```bash
./build/evaluate output/results.csv ground_truth.csv output/errors.csv
```

`ground_truth.csv` в корне размечает **три исходных синтетических** файла
из `test_images/`; номера взяты из исходного генератора, не из OCR.
Эта разметка не относится к вашим реальным кадрам.

Метрики считаются по всем строкам ground truth:

- `accuracy = exact / samples`: полное совпадение после приведения визуально
  одинаковых кириллических букв к латинице и удаления пробелов/дефисов;
- `CER = сумма расстояний Левенштейна / сумма длин эталонов`;
- `output/errors.csv`: `filename,expected,predicted,distance,reason`.

**Позиционные OCR-исправления к результатам при оценке не применяются**,
иначе метрики скрыли бы ошибки. Пропущенная строка считается ошибкой полного
совпадения, прогноз для CER — пустая строка. Лишние строки выводятся отдельно
(`extra_result`), не входят в CER/accuracy. Лишние/недостающие строки дают
код выхода 2, чтобы несовпадение датасетов не осталось незамеченным.
Дубликаты имён, неверная разметка и сломанный CSV дают код 1. Обычные ошибки
распознавания при совпадающих наборах файлов — код 0: это отчёт, не порог CI.

Допускаются отрицательные кадры с пустым эталоном. Если суммарная длина
эталонов равна нулю, CER выводится как `N/A`; accuracy остаётся определённой.
CSV поддерживает UTF-8 BOM, CRLF, кавычки, запятые и переносы внутри полей.

Утилита `evaluate` и тесты строковой логики не зависят от OpenCV/Tesseract:

```bash
cmake -S . -B build-core -DPLATE_BUILD_RECOGNIZER=OFF
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

## Синтетика и реальные кадры

```bash
./build/generate_test_image synthetic_images 3
./build/plate_recognizer synthetic_images - output/synthetic.csv --debug output/synthetic-debug
./build/evaluate output/synthetic.csv synthetic_images/ground_truth.csv output/synthetic-errors.csv
```

Генератор создаёт 1–3 изображения и парный CSV. Фиксированный seed,
перспективное преобразование всей таблички без обрезания текста, шум
добавляется в float с преобразованием назад в uint8. Это проверка механики,
не имитация реального распределения кадров и не гарантия OCR accuracy.

Реальных фотографий в предоставленном архиве нет. Для проверки создайте
отдельную папку `real_images/` и `real_ground_truth.csv` с заголовком
`filename,plate_number`, вручную разметьте каждый кадр. Включите дневные,
ночные/ИК, грязные, смазанные, наклонные и боковые ракурсы, номера разных
размеров, а также кадры без номера. Не назначайте неразборчивому номеру
пустой эталон: исключите его с явной пометкой в протоколе.

```bash
./build/plate_recognizer real_images data/haarcascade_russian_plate_number.xml output/real.csv --debug output/real-debug
./build/evaluate output/real.csv real_ground_truth.csv output/real-errors.csv
```

Не смешивайте синтетику с реальными данными в итоговой accuracy. Для сравнения
настроек используйте фиксированный отложенный набор, не подбирайте параметры
по нему. Подробности выполненных проверок находятся в `VALIDATION.md`.

## Ограничения

Поддерживается обычный однострочный формат: специальные, двухстрочные,
мотоциклетные, дипломатические и транзитные номера не поддерживаются.
На кадр выбирается один номер. Сильный ракурс, отсутствие видимой рамки,
блики, грязь, ночь, малое разрешение и надпись RUS рядом с номером могут
привести к отказу или ошибке. Валидный формат не доказывает существование
номера и не должен самостоятельно служить разрешением въезда.
Трекинга, голосования по нескольким кадрам и обученного DNN-детектора нет.
Точные метрики нового OCR на реальных кадрах пока не получены.

## Источники по совместимости

- [Официальное руководство OpenCV 4 → 5](https://github.com/opencv/opencv/wiki/OpenCV-4-to-5-migration)
- [OpenCV 5: каскад из xobjdetect](https://docs.opencv.org/5.0/tutorials_contrib/xobjdetect/cascade_classifier/cascade_classifier.html)
- [MSYS2: OpenCV UCRT64](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-opencv)
- [MSYS2: Tesseract UCRT64](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-tesseract-ocr)
- [MSYS2: английская модель OCR](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-tesseract-data-eng)
