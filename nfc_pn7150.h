#ifndef NFC_PN7150_H
#define NFC_PN7150_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "linux_nfc_api.h"

typedef enum eDevState
{
    eDevState_NONE,
    eDevState_WAIT_ARRIVAL,
    eDevState_PRESENT,
    eDevState_WAIT_DEPARTURE,
    eDevState_DEPARTED,
    eDevState_EXIT
}eDevState;

typedef enum eSnepClientState
{
    eSnepClientState_WAIT_OFF,
    eSnepClientState_OFF,
    eSnepClientState_WAIT_READY,
    eSnepClientState_READY,
    eSnepClientState_EXIT
}eSnepClientState;

typedef enum eHCEState
{
    eHCEState_NONE,
    eHCEState_WAIT_DATA,
    eHCEState_DATA_RECEIVED,
    eHCEState_EXIT
}eHCEState;

typedef enum eDevType
{
    eDevType_NONE,
    eDevType_TAG,
    eDevType_P2P,
    eDevType_READER
}eDevType;

typedef enum T4T_NDEF_EMU_state_t
{
    Ready,
    NDEF_Application_Selected,
    CC_Selected,
    NDEF_Selected
} T4T_NDEF_EMU_state_t;

typedef enum _eResult
{
    FRAMEWORK_SUCCESS,
    FRAMEWORK_FAILED
}eResult;

typedef struct tLinuxThread
{
    pthread_t thread;
    void* ctx;
    void* (*threadedFunc)(void *);
    void* mutexCanDelete;
} tLinuxThread_t;

typedef struct sMemInfo
{
    uint32_t magic;
    size_t size;
} sMemInfo_t;

typedef struct sMemInfoEnd
{
    uint32_t magicEnd;
} sMemInfoEnd_t;

typedef struct tLinuxMutex
{
    pthread_mutex_t *lock;
    pthread_cond_t  *cond;
}tLinuxMutex_t;

class nfc_pn7150 : public QThread
{
    Q_OBJECT
public:
    explicit nfc_pn7150(QObject *parent);
    ~nfc_pn7150();

    /********************************** CallBack **********************************/

    int InitEnv();
    void PrintNDEFContent(nfc_tag_info_t* TagInfo, ndef_info_t* NDEFinfo, unsigned char* ndefRaw, unsigned int ndefRawLen);
    int LookForTag(char** args, int args_len, char* tag, char** data, int format);
    int InitMode(int tag, int p2p, int hce);
    int DeinitPollMode();
    int SnepPush(unsigned char* msgToPush, unsigned int len);
    int WaitDeviceArrival(int mode, unsigned char* msgToSend, unsigned int len);
    void strtolower(char * string);
    char* strRemovceChar(const char* str, char car);
    int convertParamtoBuffer(char* param, unsigned char** outBuffer, unsigned int* outBufferLen);
    void cmd_poll();
    int CleanEnv();

    bool nfcThread = true;
    QObject * parentThread;

protected:
    void run();

signals:
    void sigMsg(QString, int);
    void sigCardData(QByteArray);

public slots:
    void sltNfcEndThread();
};

#endif // NFC_PN7150_H
