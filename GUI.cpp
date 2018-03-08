#include "GUI.h"
#include "ui_GUI.h"

GUI::GUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::GUI)
{
    ui->setupUi(this);
    connectivityManager = new manager(this);
}

GUI::~GUI()
{
    delete ui;
}

void GUI::sltStatusBar(QString msg, int timeOut)
{
    ui->statusBar->showMessage(msg,timeOut);
}
