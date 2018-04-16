#include "manager.h"

manager::manager(QObject *parent) : QObject(parent)
{
    parentThread = parent;

    bluetoothDevice = new QBluetoothLocalDevice();
    nfcDevice = new nfc_pn7150(this);

    QObject::connect(this, SIGNAL(sigNfcEndThread()), nfcDevice, SLOT(sltNfcEndThread()));
    QObject::connect(nfcDevice, SIGNAL(sigMsg(QString,int)), parentThread, SLOT(sltStatusBar(QString,int)));
    QObject::connect(this, SIGNAL(sigMsg(QString,int)), parentThread, SLOT(sltStatusBar(QString,int)));
    QObject::connect(this, SIGNAL(sigUpdateBatteryStatus(QString)), parentThread, SLOT(sltUpdateBatteryStatus(QString)));

    nfcDevice->start();

    qDebug() << bluetoothDevice->isValid();
}

manager::~manager()
{
    nfcDevice->quit();
    delete nfcDevice;
    delete bluetoothDevice;
    delete batReadTimer;
}

void manager::sltCardData(QString dataIn)
{
    ndefPayload = dataIn;
    emit sigMsg(dataIn, 10000);
    deviceMACAddress = dataIn;
    if (bluetoothDevice->isValid())
    {
        //sltDisconnect();
        startDeviceDiscovery();
    }
}

void manager::sltAlarm1()
{
    qDebug() << "Alarm" << bleController->state();
    if (QLowEnergyController::DiscoveredState == bleController->state())
    {
        qDebug() << bleController->services().length();
        for (int i = 0; i < bleController->services().length(); i++)
        {
            qDebug() << bleController->services().at(i).toString() << ledUUID;
            bleService = bleServiceList->at(i);
            if (bleController->services().at(i).toString() == ledUUID)
            {
                foreach(QLowEnergyCharacteristic c, bleService->characteristics())
                {
                    if(!c.isValid())
                    {
                        continue;
                    }
                    if (writeChar == QByteArray::fromHex("1"))
                    {
                        writeChar = QByteArray::fromHex("0");
                    }
                    else
                    {
                        writeChar = QByteArray::fromHex("1");
                    }
                    qDebug() << writeChar;
                    qDebug() << c.uuid().toString();
                    bleService->writeCharacteristic(c, writeChar, QLowEnergyService::WriteWithoutResponse);
                }
            }
        }
    }
    else
    {
        // do nothing if no bluetooth device is connected and discovered.
        qDebug() << "connected state not it is";
    }
}

void manager::sltBatteryUpdate()
{
    emit sigMsg("Bat Update", 3000);
    if (QLowEnergyController::DiscoveredState == bleController->state())
    {
        for (int i = 0; i < bleController->services().length(); i++)
        {
            //qDebug() << bleController->services().at(i).toString() << batUUID;
            if (bleController->services().at(i).toString() == batUUID)
            {
                foreach(QLowEnergyCharacteristic c, bleService->characteristics())
                {
                    if(!c.isValid())
                    {
                        continue;
                    }
                    if(c.properties() & QLowEnergyCharacteristic::Read)
                    {
                        qDebug() << "Connect char read";
                        qDebug() << c.uuid().toString();
                        connect(bleService, SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)), this, SLOT(sltBatteryStatusRead(QLowEnergyCharacteristic,QByteArray)));
                        bleService->readCharacteristic(c);
                    }
                }
            }
        }
    }
    else
    {
        // do nothing if no bluetooth device is connected and discovered.
        qDebug() << "connected state not it is";
    }
}

void manager::sltBatteryStatusRead(QLowEnergyCharacteristic c, QByteArray data)
{
    if (c.uuid().toString() == "{f0001131-0451-4000-b000-000000000000}")
    {
        QByteArray data2 = data.toHex();
        bool *ok = Q_NULLPTR;
        int base = 16;
        emit sigUpdateBatteryStatus(QString::number(data2.toUInt(ok,base)));
    }
}

/*
foreach(QLowEnergyCharacteristic c, bleService->characteristics())
{
    QLowEnergyDescriptor d = c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
    if(!c.isValid()){
        continue;
    }
    QByteArray hexChar = QByteArray::fromHex("2");
    if(c.properties() & QLowEnergyCharacteristic::Read)
    { // enable read
        bleService->writeDescriptor(d, hexChar);
    }
    hexChar = QByteArray::fromHex("8");
    if(c.properties() & QLowEnergyCharacteristic::Write)
    { // enable write
    bleService->writeDescriptor(d, hexChar);
}
*/

void manager::sltBluetoothConnect()
{
    if (bluetoothDevice->isValid())
    {
        startDeviceDiscovery();
    }
}

void manager::sltDisconnect()
{
    switch(bleController->state())
    {
        case QLowEnergyController::UnconnectedState:
        {
            break;
        }
        case QLowEnergyController::ConnectingState:
        {
            break;
        }
        case QLowEnergyController::ConnectedState:
        {
            break;
        }
        case QLowEnergyController::DiscoveringState:
        {
            break;
        }
        case QLowEnergyController::DiscoveredState:
        {
            while (!bleServiceList->isEmpty())
            {
                delete bleServiceList->front();
                bleServiceList->pop_front();
            }
            bleController->disconnectFromDevice();
            delete bleServiceList;
            delete bleService;
            delete bleController;
            break;
        }
        case QLowEnergyController::ClosingState:
        {
            break;
        }
        case QLowEnergyController::AdvertisingState:
        {
            break;
        }
    }
}

void manager::startDeviceDiscovery()
{
    // Create a discovery agent and connect to its signals
    discoveryAgent = new QBluetoothDeviceDiscoveryAgent();
    // Connect signals for discoveryAgent
    connect(discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)), this, SLOT(sltDeviceDiscovered(QBluetoothDeviceInfo)));
    // Start a discovery
    discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

// In your local slot, read information about the found devices
void manager::sltDeviceDiscovered(const QBluetoothDeviceInfo &device)
{
    qDebug() << "Found new device:" << device.name() << device.address().toString();
    if (device.address().toString() == deviceMACAddress)
    {
        emit sigMsg(device.address().toString(), 3000);
        discoveryAgent->stop();
        bleController = new QLowEnergyController(device,this);
        connect(bleController, SIGNAL(connected()), this, SLOT(sltDeviceConnected()));
        bleController->connectToDevice();
    }
}

void manager::sltDeviceConnected()
{
    emit sigMsg("BLE Device Connected", 3000);
    qDebug() << "BLE Device Connected";
    connect(bleController, SIGNAL(serviceDiscovered(QBluetoothUuid)), this, SLOT(sltServiceDiscovered(QBluetoothUuid)));
    connect(bleController, SIGNAL(discoveryFinished()), this, SLOT(sltDeviceDiscoveryFinished()));
    bleController->discoverServices();
}

void manager::sltServiceDiscovered(const QBluetoothUuid &gatt)
{
    emit sigMsg(gatt.toString(),5000);
    qDebug() << "Service discovered" << gatt.toString();
}

void manager::sltDeviceDiscoveryFinished()
{
    emit sigMsg("DeviceDiscoveryFinished", 3000);
    bleServiceList = new QList<QLowEnergyService*>;
    for (int i = 0; i < bleController->services().length(); i++)
    {
        bleServiceList->append(bleController->createServiceObject(bleController->services().at(i), this));
    }

    serviceNum = 0;
    connect(this, SIGNAL(sigServiceDetailsLoop(int)), this, SLOT(sltServiceDetailsLoop(int)));
    emit sigServiceDetailsLoop(serviceNum);
}

void manager::sltServiceDetailsLoop(int num)
{
    if (bleServiceList->length() > num)
    {
        bleService = bleServiceList->at(num);
        connect(bleService, SIGNAL(stateChanged(QLowEnergyService::ServiceState)), this, SLOT(sltServiceDetailsDiscovered(QLowEnergyService::ServiceState)));
        connect(bleService, SIGNAL(error(QLowEnergyService::ServiceError)), this, SLOT(sltBLEServiceError(QLowEnergyService::ServiceError)));
        if (bleService->includedServices().length() > 0)
        {
            for (int i = 0; i < bleService->includedServices().length(); i++)
            {
                qDebug() << bleService->includedServices().at(i).toString() << "Included Services";
            }
        }
        else
        {
            bleService->discoverDetails();
        }
    }
    else
    {
        emit sigMsg("All Service Details Discovered", 10000);
        qDebug() << "Im done!" << num;
        batReadTimer = new QTimer();
        //batReadTimer->setSingleShot(true);
        connect(batReadTimer, SIGNAL(timeout()), this, SLOT(sltBatteryUpdate()));
        batReadTimer->start(1000);
    }
}

void manager::sltBLEServiceError(QLowEnergyService::ServiceError error)
{
    qDebug() << error;
}

void manager::sltServiceDetailsDiscovered(QLowEnergyService::ServiceState newState)
{
    //qDebug() << newState << "ServiceDetailsFunctionCall";
    if (newState == QLowEnergyService::ServiceDiscovered)
    {
        emit sigMsg("Service Discovered", 5000);
        qDebug() << "Service Discovered";
        serviceNum = serviceNum + 1;
        emit sigServiceDetailsLoop(serviceNum);
    }
    else if (newState == QLowEnergyService::InvalidService)
    {
        emit sigMsg("Invalid Service", 5000);
        qDebug() << "Invalid Service";
    }
    else if (newState == QLowEnergyService::DiscoveringServices)
    {
        emit sigMsg("Discovering Services", 5000);
        qDebug() << "Discovering Services";
    }
    else if (newState == QLowEnergyService::DiscoveryRequired)
    {
        emit sigMsg("Discovery Requited",5000);
        qDebug() << "Discovery Requited";
    }
    else
    {
        emit sigMsg("Other Service State", 5000);
        qDebug() << "Other Service State";
    }
}

/*
    foreach(QLowEnergyCharacteristic c, bleService->characteristics())
    {
    QLowEnergyDescriptor d = c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
    if(!c.isValid()){
        continue;
    }
    QByteArray hexChar = QByteArray::fromHex("2");
    if(c.properties() & QLowEnergyCharacteristic::Read)
    { // enable read
        bleService->writeDescriptor(d, hexChar);
    }
    hexChar = QByteArray::fromHex("8");
    if(c.properties() & QLowEnergyCharacteristic::Write)
    { // enable write
        bleService->writeDescriptor(d, hexChar);
    }
    */

