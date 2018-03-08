#ifndef GUI_H
#define GUI_H

#include <QMainWindow>
#include "manager.h"

namespace Ui {
class GUI;
}

class GUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit GUI(QWidget *parent = 0);
    ~GUI();

    manager * connectivityManager;

private:
    Ui::GUI *ui;

signals:

public slots:
    void sltStatusBar(QString msg,int timeOut);
};

#endif // GUI_H
