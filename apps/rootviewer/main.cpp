#include "rootcanvas.h"
#include "rootviewerwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QMainWindow>
#include <QMouseEvent>
#include <TROOT.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    ROOT::EnableThreadSafety();
    QApplication application(argc, argv);
    if (argc == 2 && std::string(argv[1]) == "--surface-smoke") {
        GetDAQ::RootEvent event;
        for (int aget = 0; aget < 4; ++aget) {
            for (int channel = 0; channel < 68; ++channel) {
                std::vector<GetDAQ::RootSample> &samples = event.channels[aget][channel];
                samples.reserve(512);
                for (int bucket = 0; bucket < 512; ++bucket) {
                    const double phase = (bucket + channel * 3.0 + aget * 17.0) / 31.0;
                    const unsigned value = static_cast<unsigned>(2048.0 + 1900.0 * std::sin(phase));
                    samples.push_back(GetDAQ::RootSample{static_cast<std::uint16_t>(bucket),
                                                         static_cast<std::uint16_t>(value)});
                    ++event.sampleCount;
                }
                ++event.channelCount;
            }
        }
        EventCanvas canvas;
        canvas.resize(1180, 820);
        canvas.SetSurface(event);
        canvas.show();
        application.processEvents();
        QImage before(canvas.size(), QImage::Format_RGB32);
        before.fill(Qt::white);
        canvas.render(&before);
        std::size_t coloredPixels = 0;
        for (int y = 0; y < before.height(); ++y)
            for (int x = 0; x < before.width(); ++x)
                if (before.pixelColor(x, y) != QColor(250, 250, 250)) ++coloredPixels;
        if (coloredPixels < 10000) return 2;

        QMouseEvent press(QEvent::MouseButtonPress, QPointF(200, 200), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&canvas, &press);
        QMouseEvent move(QEvent::MouseMove, QPointF(280, 250), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&canvas, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(280, 250), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&canvas, &release);
        application.processEvents();
        QImage rotated(canvas.size(), QImage::Format_RGB32);
        rotated.fill(Qt::white);
        canvas.render(&rotated);
        std::size_t changedPixels = 0;
        for (int y = 0; y < before.height(); ++y)
            for (int x = 0; x < before.width(); ++x)
                if (before.pixel(x, y) != rotated.pixel(x, y)) ++changedPixels;
        if (changedPixels < 1000) return 3;
        std::cout << "PASS ROOT surface paint and mouse rotation; changed_pixels=" << changedPixels << '\n';
        return 0;
    }
    std::unique_ptr<QMainWindow> window = CreateRootViewerWindow();
    window->show();
    return application.exec();
}
