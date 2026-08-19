# Shuttle Engine

Shuttle Engine — экспериментальный Vulkan-рендерер и редактор сцен с GPU-driven pipeline, созданный на C++.

Проект объединяет низкоуровневый рендеринг на Vulkan, систему загрузки сцен и окружений, а также редакторский интерфейс для просмотра и настройки результата в реальном времени.

## Overview

Основная цель проекта — исследовать современные подходы к построению realtime-рендерера:

- Vulkan 1.4;
- GPU-driven rendering;
- indirect indexed drawing;
- bindless descriptors;
- compute passes для подготовки команд отрисовки;
- HDR-окружения и IBL;
- debug-режимы визуализации;
- собственный editor UI.

## Features

### Rendering

- Vulkan-based rendering backend.
- Динамическое создание и пересоздание swapchain.
- Отдельные render pass для вычислительных и графических этапов.
- Индексированная indirect-отрисовка.
- Поддержка нескольких кадров, находящихся в обработке.
- Управление ресурсами GPU с отложенным освобождением.

### GPU-driven pipeline

Текущий pipeline подготовки сцены включает:

1. обновление мировых трансформаций;
2. подсчёт экземпляров мешей;
3. prefix sum;
4. построение remap-буфера экземпляров;
5. подготовку indirect draw commands;
6. основной проход отрисовки.

Такой подход позволяет перенести значительную часть подготовки данных на GPU и уменьшить объём работы CPU перед отрисовкой.

### Assets

Редактор поддерживает работу с двумя основными типами ресурсов:

- сценами;
- окружениями.

Для сцен поддерживаются:

- загрузка предварительно скомпилированных файлов;
- импорт FBX/glTF;
- загрузка геометрии, материалов и текстур;
- компрессия текстур в блочные форматы.
- отображение количества мешей и материалов;
- отслеживание несохранённых изменений.

Для окружений поддерживаются:

- загрузка скомпилированных environment-файлов;
- импорт HDR-карт;
- генерация данных для image-based lighting;
- настройка размеров карт и количества samples.

### Editor UI

Редактор предоставляет:

- кастомный title bar;
- меню `File`;
- открытие сцен и окружений;
- импорт ассетов;
- диалог сохранения;
- список загруженных сцен и окружений;
- выбор активного ассета;
- безопасное удаление ассетов;
- настройки камеры;
- настройки renderer;
- debug viewport layouts;
- FPS overlay.

## Debug Rendering

В renderer предусмотрены дополнительные режимы визуализации, включая:

- Final;
- Albedo;
- Normal;
- Tangent;
- Bitangent;
- Metallic;
- Roughness;
- Ambient Occlusion;
- Emissive;
- UV;
- Mesh ID;
- Material ID;
- Instance ID;
- View Depth;
- Linear Depth;
- World Position;
- World Normal.

В зависимости от выбранной раскладки debug viewport может отображать один, два или четыре выходных изображения.

## Architecture

Проект разделён на несколько логических уровней.

### Application

`Application` управляет жизненным циклом приложения:

- инициализацией SDL;
- созданием Vulkan instance;
- выбором physical device;
- созданием logical device;
- инициализацией allocator;
- созданием swapchain;
- настройкой frame manager;
- созданием render passes;
- запуском главного цикла;
- обработкой событий окна.

### MainWindow

`MainWindow` отвечает за редакторский интерфейс:

- отображение меню;
- работу с file dialogs;
- отображение viewport;
- настройки камеры;
- настройки renderer;
- управление списком ассетов;
- взаимодействие с callback-функциями приложения.

### Render passes

В проекте используются отдельные этапы для подготовки и отображения сцены:

- `WorldTransformUpdatePass`;
- `MeshInstancesCountPass`;
- `PrefixSumPass`;
- `InstanceRemapPass`;
- `MainRenderPass`;
- `UiPass`.

### Resource lifetime

GPU-ресурсы, связанные с viewport и swapchain, не уничтожаются непосредственно во время активного кадра.

Вместо этого они передаются в `ResourceBin`, где освобождаются после завершения соответствующего frame slot. Такой подход позволяет избежать преждевременного уничтожения ресурсов, которые ещё могут использоваться GPU.

## Controls

| Действие | Управление |
|---|------------|
| Перемещение камеры | `WASD`     |
| Вертикальное перемещение | `Q` / `E`  |
| Вращение камеры | `Arrow Keys`    |
| Выход | `Escape`   |

## Asset workflow

### Open scene

1. Открыть меню `File`.
2. Выбрать `Open Scene`.
3. Указать скомпилированный файл сцены.
4. Дождаться загрузки GPU-ресурсов.
5. Сцена появится в разделе `SCENES`.

### Open environment

1. Открыть меню `File`.
2. Выбрать `Open Environment`.
3. Указать environment-файл.
4. Окружение появится в разделе `ENVIRONMENTS`.

### Import scene

1. Выбрать `Import Scene`.
2. Указать FBX или glTF-файл.
3. Настроить параметры импорта.
4. Запустить импорт.
5. Сцена будет скомпилирована и загружена в память.

### Import environment

1. Выбрать `Import Environment`.
2. Указать HDR-файл.
3. Настроить параметры IBL.
4. Запустить импорт.
5. Environment-ресурс будет сгенерирован и загружен.

## Build

Проект использует CMake.

Пример стандартной сборки:

```bash
bash

git clone 

cd ShuttleEngine

cmake -S . -B build

cmake --build build --config Release
```

Конкретные требования к Vulkan SDK, SDL2 и остальным зависимостям зависят от конфигурации проекта и платформы.

## Dependencies

Проект использует или интегрируется со следующими технологиями и библиотеками:

- C++;
- Vulkan;
- Vulkan-Hpp;
- SDL2;
- Dear ImGui;
- GLM;
- Vulkan Memory Allocator;
- VkBootstrap;
- portable-file-dialogs.

Перед сборкой необходимо убедиться, что все зависимости доступны в конфигурации CMake проекта.

## Project Status

### Implemented

- Vulkan initialization.
- Swapchain creation and recreation.
- Frame synchronization.
- GPU-driven scene rendering.
- Indirect drawing pipeline.
- Scene loading.
- Environment loading.
- FBX/glTF scene import.
- HDR environment import.
- Asset selection and removal.
- Save dialog integration.
- Camera controls.
- Renderer settings.
- Debug output layouts.
- Custom editor UI.

### Not implemented

- Scene hierarchy editor.
- Scene node selection and manipulation.
- Animation playback.
- Full editor scene manipulation workflow.
- Advanced post-processing pipeline.

## Roadmap

Возможные направления развития:

- редактор иерархии сцены;
- выбор объектов во viewport;
- трансформационные gizmos;
- поддержка анимаций;
- более развитая система материалов;
- полноценное сохранение environment-ассетов;
- улучшенная обработка ошибок импорта;
- автоматизированные тесты;
- профилирование GPU и CPU;
- поддержка дополнительных форматов ресурсов.

## Technical Focus

Проект создан как исследовательский и портфолио-проект с фокусом на:

- современный Vulkan API;
- управление GPU-ресурсами;
- GPU-driven rendering;
- асинхронную подготовку данных;
- работу с indirect draw commands;
- построение редакторского инструментария;
- разделение application, rendering и editor layers.

## Screenshots and Demo

Сюда можно добавить скриншоты и видео:

## License

Собственный код проекта распространяется под лицензией MIT.

Полный текст лицензии находится в файле [`LICENSE`](LICENSE).

Лицензии сторонних библиотек и компонентов должны рассматриваться отдельно в соответствии с их исходными условиями распространения.

## Author

**Shagu**

Shuttle Engine создаётся как технический проект для изучения realtime-графики, Vulkan и GPU-driven rendering.
