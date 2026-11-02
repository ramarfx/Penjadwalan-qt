#include <QApplication>
#include <QStyleFactory>

#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Penjadwalan Mata Kuliah");
    QApplication::setApplicationDisplayName("Penjadwalan Mata Kuliah");
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();
    return app.exec();
}
