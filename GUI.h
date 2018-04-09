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
    void sigBtnPairingPressed();
    void sigBtnDisconnectPressed();
    void sigAlarm1();

public slots:
    void btnPairingPressed();
    void btnDisconnectPressed();
    void btnAlarm1Pressed();
    void btnAlarm2Pressed();
    void btnAlarm3Pressed();
    void sltStatusBar(QString msg,int timeOut);
};

#endif // GUI_H
