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
    bluetoothConnect();
}

manager::~manager()
{
    nfcDevice->quit();
    delete nfcDevice;
    delete bluetoothDevice;
}

void manager::sltCardData(QByteArray dataIn)
{
    ndefPayload = dataIn;
    QString data = dataIn;
    emit sigMsg(data, 10000);
}

void manager::bluetoothConnect()
{
    QBluetoothAddress trentonPhone("C0EEFB5C42D3");
    if (true)
    {
        bluetoothDevice->requestPairing(trentonPhone,QBluetoothLocalDevice::AuthorizedPaired);
    }

    bleController.createCentral(&service);
}

void manager::startDeviceDiscovery()
{

    // Create a discovery agent and connect to its signals
    discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)), this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));
    // Start a discovery
    discoveryAgent->start();
    //...
}

// In your local slot, read information about the found devices
void manager::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    qDebug() << "Found new device:" << device.name() << '(' << device.address().toString() << ')';
}

/*




m_control = new QLowEnergyController(m_currentDevice->getDevice(), this);
connect(m_control, &QLowEnergyController::serviceDiscovered,
        this, &DeviceHandler::serviceDiscovered);
connect(m_control, &QLowEnergyController::discoveryFinished,
        this, &DeviceHandler::serviceScanDone);

connect(m_control, static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
        this, [this](QLowEnergyController::Error error) {
    Q_UNUSED(error);
    setError("Cannot connect to remote device.");
});
connect(m_control, &QLowEnergyController::connected, this, [this]() {
    setInfo("Controller connected. Search services...");
    m_control->discoverServices();
});
connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
    setError("LowEnergy controller disconnected");
});

// Connect
m_control->connectToDevice();



void BLEAdvertisementCommunicator::startAdvertisingService() {
    mController = QLowEnergyController::createPeripheral(this);
    mAdvertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    mAdvertisingData.setIncludePowerLevel(false);
    mAdvertisingData.setLocalName("MyApplication");
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(QBluetoothUuid(serviceUuid));

    auto service = mController->addService(serviceData, mController);

    connect(mController, SIGNAL(connected()), this, SLOT(onDeviceConnected()));
    connect(mController, SIGNAL(disconnected()), this, SLOT(onDeviceDisconnected()));
    connect(mController, SIGNAL(error(QLowEnergyController::Error)), this, SLOT(onError(QLowEnergyController::Error)));
    mResponseData.setServices({QBluetoothUuid(serviceUuid)});
    mController->startAdvertising(QLowEnergyAdvertisingParameters(), mAdvertisingData,
                                  mResponseData);
}

UUID: BD4199BB-7414-CD8B-B8D2-1EC7BBF0EAF3
UUID: FFF0
Char 1: R/W - FFF1
Char 2: R - FFF2
Char 3: W - FFF3
Char 4: Notify - FFF4
Char 5: R - FFF5


*/
