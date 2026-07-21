#include <iostream>
#include <string>
#include <algorithm>

#include "SceneImporter/SceneImporter.hpp"

void printHelp() {
    std::cout << "\n=================================================================" << std::endl;
    std::cout << "[ShuttleEngine AssetProcessor] Ультимативный CLI-компилятор" << std::endl;
    std::cout << "Использование флагов:" << std::endl;
    std::cout << "  -s, --source       <путь>  : Путь к исходному файлу (.obj, .fbx, .gltf)" << std::endl;
    std::cout << "  -d, --destination  <путь>  : Путь для запекания бинарника (.scene)" << std::endl;
    std::cout << "\nПример запуска:" << std::endl;
    std::cout << "  AssetProcessor.exe -s C:/Users/Bistro.fbx -d E:/Engine/bistro.scene" << std::endl;
    std::cout << "=================================================================\n" << std::endl;
}

int main(int argc, char* argv[]) {

    std::string inputPath;
    std::string outputPath;

    // 1. Двухпроходный парсер аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-s" || arg == "--source") {
            if (i + 1 < argc) {
                inputPath = argv[++i];
            }
        } else if (arg == "-d" || arg == "--destination") {
            if (i + 1 < argc) {
                outputPath = argv[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        }
    }

    // 2. Жесткая валидация входных параметров
    if (inputPath.empty() || outputPath.empty()) {
        std::cerr << "[Error] КРИТИЧЕСКАЯ ОШИБКА: Не указаны обязательные флаги -s или -d!" << std::endl;
        printHelp();
        return -1;
    }

    std::cout << "[Main] Запуск офлайн-конвейера ShuttleEngine..." << std::endl;
    std::cout << "[Main] Исходник (Source):      " << inputPath << std::endl;
    std::cout << "[Main] Результат (Destination): " << outputPath << std::endl;

    // 3. Вызываем наш статический компилятор ресурсов, передавая ОБА пути!
    if (shuttle_engine::assets::SceneImporter::loadScene(inputPath, outputPath)) {
        std::cout << "\n=================================================================" << std::endl;
        std::cout << "[Main] ТРИУМФАЛЬНЫЙ УСПЕХ! Сцена полностью запечена." << std::endl;
        std::cout << "[Main] Итоговый файл: " << outputPath << std::endl;
        std::cout << "=================================================================\n" << std::endl;
    } else {
        std::cerr << "[Main] КРИТИЧЕСКАЯ ОШИБКА: Сбой компиляции ассета!" << std::endl;
        return -1;
    }

    return 0;
}
