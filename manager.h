#ifndef MANAGER_H
#define MANAGER_H

#include <QObject>
#include <QDebug>
#include <QtNfc>
#include <QtBluetooth>
#include <QSharedPointer>
#include "nfc_pn7150.h"

class manager : public QObject
{
    Q_OBJECT
public:
    explicit manager(QObject * parent);
    ~manager();

    QObject * parentThread;

    QBluetoothDeviceDiscoveryAgent * discoveryAgent;
    QBluetoothServiceInfo service;
    QBluetoothLocalDevice * bluetoothDevice;
    QLowEnergyController * bleController;
    QLowEnergyService * bleService;
    QList<QLowEnergyService*> * bleServiceList;
    nfc_pn7150 * nfcDevice;
    unsigned int serviceNum = 0;

private:
    QString ndefPayload;
    QString deviceMACAddress = "54:6C:0E:A0:39:61"; //"54:6C:0E:9B:5A:E1"; // "54:6C:0E:9B:53:A0";
    QString ledUUID = "{f0001110-0451-4000-b000-000000000000}";
    QByteArray writeChar = 0;

    void startDeviceDiscovery();

signals:
    void sigNfcEndThread();
    void sigMsg(QString,int);
    void sigServiceDetailsLoop(int num);

public slots:
    void sltCardData(QString);
    void sltDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void sltDeviceConnected();
    void sltServiceDiscovered(const QBluetoothUuid &Uuid);
    void sltServiceDetailsDiscovered(QLowEnergyService::ServiceState newState);
    void sltDeviceDiscoveryFinished();
    void sltBLEServiceError(QLowEnergyService::ServiceError error);
    void sltServiceDetailsLoop(int num);
    void sltBluetoothConnect();
    void sltDisconnect();
    void sltAlarm1();
};

#endif // MANAGER_H
