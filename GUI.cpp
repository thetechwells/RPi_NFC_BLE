#include "GUI.h"
#include "ui_GUI.h"

GUI::GUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::GUI)
{
    ui->setupUi(this);
    connectivityManager = new manager(this);

    connect(this, SIGNAL(sigBtnPairingPressed()), connectivityManager, SLOT(sltBluetoothConnect()));
    connect(this, SIGNAL(sigAlarm1()), connectivityManager, SLOT(sltAlarm1()));
    connect(this, SIGNAL(sigBtnDisconnectPressed()), connectivityManager, SLOT(sltDisconnect()));
}

GUI::~GUI()
{
    delete ui;
    delete connectivityManager;
}

void GUI::sltStatusBar(QString msg, int timeOut)
{
    ui->statusBar->showMessage(msg,timeOut);
}

void GUI::btnPairingPressed()
{
    emit sigBtnPairingPressed();
}

void GUI::btnDisconnectPressed()
{
    emit sigBtnDisconnectPressed();
}

void GUI::btnAlarm1Pressed()
{
    emit sigAlarm1();
}

void GUI::btnAlarm2Pressed()
{

}

void GUI::btnAlarm3Pressed()
{

}


