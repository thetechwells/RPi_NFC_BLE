#include "manager.h"

manager::manager(QObject *parent) : QObject(parent)
{
    parentThread = parent;

    bluetoothDevice = new QBluetoothLocalDevice();
    nfcDevice = new nfc_pn7150(this);

    QObject::connect(this, SIGNAL(sigNfcEndThread()), nfcDevice, SLOT(sltNfcEndThread()));
    QObject::connect(nfcDevice, SIGNAL(sigMsg(QString,int)), parentThread, SLOT(sltStatusBar(QString,int)));
    QObject::connect(this, SIGNAL(sigMsg(QString,int)), parentThread, SLOT(sltStatusBar(QString,int)));

    nfcDevice->start();

    qDebug() << bluetoothDevice->isValid();
}

manager::~manager()
{
    nfcDevice->quit();
    delete nfcDevice;
    delete bluetoothDevice;
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
    if (true) //QLowEnergyController::ConnectedState == bleController->state())
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
                    if (writeChar == QByteArray::fromHex("1"))
                    {
                        writeChar = QByteArray::fromHex("0");
                    }
                    else
                    {
                        writeChar = QByteArray::fromHex("1");
                    }
                    qDebug() << c.uuid().toString();
                    bleService->writeCharacteristic(c, writeChar, QLowEnergyService::WriteWithoutResponse);
                }
            }
        }
    }
    else
    {
        qDebug() << "connected state not is it";
    }
}

/* foreach(QLowEnergyCharacteristic c, bleService->characteristics())
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
    if (QLowEnergyController::ConnectedState == bleController->state())
    {
        while (!bleServiceList->isEmpty())
        {
            delete bleServiceList->front();
            bleServiceList->pop_front();
        }
        bleController->disconnectFromDevice();
    }
    while (QLowEnergyController::UnconnectedState != bleController->state())
    {

    }
    delete bleController;
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
    //if (device.address().toString() == "54:6C:0E:9B:5A:E1")
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
    //qDebug() << bleServiceList->length();
    connect(this, SIGNAL(sigServiceDetailsLoop(int)), this, SLOT(sltServiceDetailsLoop(int)));
    emit sigServiceDetailsLoop(serviceNum);
}

void manager::sltServiceDetailsLoop(int num)
{
    //qDebug() << "Im in" << num;
    if (bleServiceList->length() > num)
    {
        //qDebug() << "Im in further" << num;
        bleService = bleServiceList->at(num);
        connect(bleService, SIGNAL(stateChanged(QLowEnergyService::ServiceState)), this, SLOT(sltServiceDetailsDiscovered(QLowEnergyService::ServiceState)));
        connect(bleService, SIGNAL(error(QLowEnergyService::ServiceError)), this, SLOT(sltBLEServiceError(QLowEnergyService::ServiceError)));
        //qDebug() << "Im includedServices" << bleService->includedServices().length();
        if (bleService->includedServices().length() > 0)
        {
            for (int i = 0; i < bleService->includedServices().length(); i++)
            {
                //qDebug() << bleService->includedServices().at(i).toString() << "Included Services";
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
        emit sigMsg("Discoversing Services", 5000);
        qDebug() << "Discoversing Services";
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

