/**
* @file main.cpp
 * @brief Application entry point.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Application/Application.hpp"

using namespace shuttle;

/**
 * @brief Application entry point.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char** argv)
{
    try
    {
        engine::Application app(argc, argv);
        return app.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[CRITICAL ERROR] " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
