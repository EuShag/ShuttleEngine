//
// Created by Shagu on 04.08.2026.
//

#ifndef SHUTTLEENGINE_MAINWINDOW_HPP
#define SHUTTLEENGINE_MAINWINDOW_HPP

namespace shuttle::editor::core {
    class MainWindow {
    public:
        void drawUi();
    private:
        void drawMenuBar();
        void drawTitleBar();
        void drawDockSpace();

        void drawSceneExplorer();
        void drawInspector();
        void drawViewport();
    };
}

#endif //SHUTTLEENGINE_MAINWINDOW_HPP
