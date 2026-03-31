#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GladiusModTool");
    app.setOrganizationName("GladiusModTool");
    app.setApplicationVersion("0.7.0");

    MainWindow w;
    w.show();

    return app.exec();
}
