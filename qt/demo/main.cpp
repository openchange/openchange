#include "demoapp.h"

#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    DemoApp *demo = new DemoApp();
    demo->show();

    return app.exec();
}
