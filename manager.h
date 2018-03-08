#ifndef MANAGER_H
#define MANAGER_H

#include <QObject>
#include <QDebug>
#include <QtNfc>
#include <QtBluetooth>
#include "nfc_pn7150.h"

class manager : public QObject
{
    Q_OBJECT
public:
    explicit manager(QObject * parent);
    ~manager();

    QObject * parentThread;

    QBluetoothServiceDiscoveryAgent *discoveryAgent;
    QBluetoothServiceInfo service;
    QBluetoothLocalDevice * bluetoothDevice;
    QLowEnergyController bleController;
    nfc_pn7150 * nfcDevice;

private:
    QByteArray ndefPayload;

    void bluetoothConnect();
    void startDeviceDiscovery();

signals:
    void sigNfcEndThread();
    void sigMsg(QString,int);

public slots:
    void sltCardData(QByteArray);
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
};

#endif // MANAGER_H
