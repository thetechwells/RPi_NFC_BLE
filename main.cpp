#include "GUI.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GUI w;
    //QObject::connect(w, SIGNAL, w, SLOT(close()));
    w.show();

    return a.exec();
}
