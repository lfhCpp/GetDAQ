#include <QApplication>
#include <QMainWindow>
#include <cstring>
#include <memory>
#include "waveformviewer_cli.h"
#include "waveformwindow.h"

int main(int argc, char *argv[])
{
    int commandExitCode = 0;
    if (RunWaveformViewerCommandLine(argc, argv, &commandExitCode)) return commandExitCode;

    bool surfaceSmoke = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--surface-smoke") == 0) surfaceSmoke = true;
    }
    QApplication application(argc, argv);
    if (surfaceSmoke) return GetDAQ::RunWaveformSurfaceSmoke();
    std::unique_ptr<QMainWindow> window(GetDAQ::CreateWaveformWindow());
    window->show();
    return application.exec();
}
