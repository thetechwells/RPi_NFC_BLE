#include "nfc_pn7150.h"

static void* g_ThreadHandle = NULL;
static void* g_devLock = NULL;
static void* g_SnepClientLock = NULL;
static eDevState g_DevState = eDevState_NONE;
static eDevType g_Dev_Type = eDevType_NONE;
static eSnepClientState g_SnepClientState = eSnepClientState_OFF;
static nfc_tag_info_t g_TagInfo;
static nfcTagCallback_t g_TagCB;
static nfcSnepServerCallback_t g_SnepServerCB;
static nfcSnepClientCallback_t g_SnepClientCB;

nfc_pn7150::nfc_pn7150(QObject *parent) : QThread(parent)
{
    parentThread = parent;
}

nfc_pn7150::~nfc_pn7150()
{

}

/************************ Tools ****************************/

void nfc_pn7150::run()
{
    QObject::connect(this, SIGNAL(sigCardData(QString)), parentThread, SLOT(sltCardData(QString)));
    cmd_poll();
}

void * framework_GetCurrentThreadId()
{
    return (void*)pthread_self();
}

void * framework_GetThreadId(void * threadHandle)
{
    tLinuxThread_t *linuxThread = (tLinuxThread_t*)threadHandle;
    return (void*)linuxThread->thread;
}

void framework_MilliSleep(uint32_t ms)
{
    usleep(1000*ms);
}

/*********************** Mutex ****************************/

void framework_LockMutex(void * mutexHandle)
{
    tLinuxMutex_t *mutex = (tLinuxMutex_t*)mutexHandle;
    int res = pthread_mutex_lock(mutex->lock);
    if (res)
    {
        qDebug() << "lock() failed errno";
    }
}

void framework_UnlockMutex(void * mutexHandle)
{
    tLinuxMutex_t *mutex = (tLinuxMutex_t*)mutexHandle;
    int res = pthread_mutex_unlock(mutex->lock);
    if (res)
    {
        qDebug() << "unlock() failed";
    }
}

void framework_WaitMutex(void * mutexHandle, uint8_t needLock)
{
    tLinuxMutex_t *mutex = (tLinuxMutex_t*)mutexHandle;
    if (needLock)
    {
        framework_LockMutex(mutexHandle);
    }
    pthread_cond_wait(mutex->cond,mutex->lock);
    if (needLock)
    {
        framework_UnlockMutex(mutexHandle);
    }
}

void framework_NotifyMutex(void * mutexHandle, uint8_t needLock)
{
    tLinuxMutex_t *mutex = (tLinuxMutex_t*)mutexHandle;
    if (needLock)
    {
        framework_LockMutex(mutexHandle);
    }
    pthread_cond_broadcast(mutex->cond);
    if (needLock)
    {
        framework_UnlockMutex(mutexHandle);
    }
}

/*********************** Memory Mgmt ****************************/

void* framework_AllocMem(size_t size)
{
    sMemInfo_t *info = NULL;
    sMemInfoEnd_t *infoEnd = NULL;
    uint8_t * pMem = (uint8_t *) malloc(size+sizeof(sMemInfo_t)+sizeof(sMemInfoEnd_t));
    info = (sMemInfo_t*)pMem;
    info->magic = 0xDEADC0DE;
    info->size  = size;
    pMem = pMem+sizeof(sMemInfo_t);
    memset(pMem,0xAB,size);
    infoEnd = (sMemInfoEnd_t*)(pMem+size);
    infoEnd->magicEnd = 0xDEADC0DE;
    return pMem;
}

void framework_FreeMem(void *ptr)
{
    if(NULL !=  ptr)
    {
        sMemInfoEnd_t *infoEnd = NULL;
        uint8_t *memInfo = (uint8_t*)ptr;
        sMemInfo_t *info = (sMemInfo_t*)(memInfo - sizeof(sMemInfo_t));
        infoEnd = (sMemInfoEnd_t*)(memInfo+info->size);
        if ((info->magic != 0xDEADC0DE)||(infoEnd->magicEnd != 0xDEADC0DE))
        {
            // Call Debugger
            *(int *)(uintptr_t)0xbbadbeef = 0;
        }else
        {
            memset(info,0x14,info->size+sizeof(sMemInfo_t)+sizeof(sMemInfoEnd_t));
        }
        free(info);
    }
}

void framework_DeleteMutex(void * mutexHandle)
{
    tLinuxMutex_t *mutex = (tLinuxMutex_t*)mutexHandle;
    pthread_mutex_destroy(mutex->lock);
    pthread_cond_destroy(mutex->cond);
    framework_FreeMem(mutex);
}

void framework_DeleteThread(void * threadHandle)
{
    tLinuxThread_t *linuxThread = (tLinuxThread_t*)threadHandle;
    framework_DeleteMutex(linuxThread->mutexCanDelete);
    framework_FreeMem(linuxThread);
}

void framework_JoinThread(void * threadHandle)
{
    tLinuxThread_t *linuxThread = (tLinuxThread_t*)threadHandle;
    if (pthread_self() != linuxThread->thread)
    {
        framework_LockMutex(linuxThread->mutexCanDelete);
        framework_UnlockMutex(linuxThread->mutexCanDelete);
    }
}

eResult framework_CreateMutex(void ** mutexHandle)
{
    tLinuxMutex_t *mutex = (tLinuxMutex_t *)framework_AllocMem(sizeof(tLinuxMutex_t));
    mutex->lock = (pthread_mutex_t*)framework_AllocMem(sizeof(pthread_mutex_t));
    mutex->cond = (pthread_cond_t*)framework_AllocMem(sizeof(pthread_cond_t));
    pthread_mutex_init(mutex->lock,NULL);
    pthread_cond_init(mutex->cond,NULL);
    *mutexHandle = mutex;
    return FRAMEWORK_SUCCESS;
}

void* thread_object_func(void* obj)
{
    tLinuxThread_t *linuxThread = (tLinuxThread_t *)obj;
    void *res = NULL;
    framework_LockMutex(linuxThread->mutexCanDelete);
    res = linuxThread->threadedFunc(linuxThread->ctx);
    framework_UnlockMutex(linuxThread->mutexCanDelete);
    return res;
}

eResult framework_CreateThread(void** threadHandle, void * (* threadedFunc)(void *) , void * ctx)
{
    tLinuxThread_t *linuxThread = (tLinuxThread_t *)framework_AllocMem(sizeof(tLinuxThread_t));
    linuxThread->ctx = ctx;
    linuxThread->threadedFunc = threadedFunc;
    framework_CreateMutex(&(linuxThread->mutexCanDelete));
    if (pthread_create(&(linuxThread->thread), NULL, thread_object_func, linuxThread))
    {
        qDebug() << "Cannot create Thread";
        framework_DeleteMutex(linuxThread->mutexCanDelete);
        framework_FreeMem(linuxThread);

        return FRAMEWORK_FAILED;
    }
    pthread_detach(linuxThread->thread);
    *threadHandle = linuxThread;
    return FRAMEWORK_SUCCESS;
}

/**************************** CallBack *********************************/

void onTagArrival(nfc_tag_info_t *pTagInfo)
{
    framework_LockMutex(g_devLock);

    if(eDevState_WAIT_ARRIVAL == g_DevState)
    {
        qDebug() << "NFC Tag Found";
        memcpy(&g_TagInfo, pTagInfo, sizeof(nfc_tag_info_t));
        g_DevState = eDevState_PRESENT;
        g_Dev_Type = eDevType_TAG;
        framework_NotifyMutex(g_devLock, 0);
    }
    else if(eDevState_WAIT_DEPARTURE == g_DevState)
    {
        memcpy(&g_TagInfo, pTagInfo, sizeof(nfc_tag_info_t));
        g_DevState = eDevState_PRESENT;
        g_Dev_Type = eDevType_TAG;
        framework_NotifyMutex(g_devLock, 0);
    }
    else if(eDevState_EXIT == g_DevState)
    {
        g_DevState = eDevState_DEPARTED;
        g_Dev_Type = eDevType_NONE;
        framework_NotifyMutex(g_devLock, 0);
    }
    else
    {
        g_DevState = eDevState_PRESENT;
        g_Dev_Type = eDevType_TAG;
    }

    framework_UnlockMutex(g_devLock);
}

void onTagDeparture(void)
{
    framework_LockMutex(g_devLock);
    if(eDevState_WAIT_DEPARTURE == g_DevState)
    {
        qDebug() << "NFC Tag Lost";
        g_DevState = eDevState_DEPARTED;
        g_Dev_Type = eDevType_NONE;
        framework_NotifyMutex(g_devLock, 0);
    }
    else if(eDevState_WAIT_ARRIVAL == g_DevState)
    {
    }
    else if(eDevState_EXIT == g_DevState)
    {
    }
    else
    {
        g_DevState = eDevState_DEPARTED;
        g_Dev_Type = eDevType_NONE;
    }
    framework_UnlockMutex(g_devLock);
}

void onDeviceArrival(void)
{
    framework_LockMutex(g_devLock);
    switch(g_DevState)
    {
        case eDevState_WAIT_DEPARTURE:
        {
            g_DevState = eDevState_PRESENT;
            g_Dev_Type = eDevType_P2P;
            framework_NotifyMutex(g_devLock, 0);
        } break;
        case eDevState_EXIT:
        {
            g_Dev_Type = eDevType_P2P;
        } break;
        case eDevState_NONE:
        {
            g_DevState = eDevState_PRESENT;
            g_Dev_Type = eDevType_P2P;
        } break;
        case eDevState_WAIT_ARRIVAL:
        {
            g_DevState = eDevState_PRESENT;
            g_Dev_Type = eDevType_P2P;
            framework_NotifyMutex(g_devLock, 0);
        } break;
        case eDevState_PRESENT:
        {
            g_Dev_Type = eDevType_P2P;
        } break;
        case eDevState_DEPARTED:
        {
            g_Dev_Type = eDevType_P2P;
            g_DevState = eDevState_PRESENT;
        } break;
    }

    framework_UnlockMutex(g_devLock);
}

void onDeviceDeparture(void)
{
    framework_LockMutex(g_devLock);

    switch(g_DevState)
    {
        case eDevState_WAIT_DEPARTURE:
        {
            g_DevState = eDevState_DEPARTED;
            g_Dev_Type = eDevType_NONE;
            framework_NotifyMutex(g_devLock, 0);
        } break;
        case eDevState_EXIT:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
        case eDevState_NONE:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
        case eDevState_WAIT_ARRIVAL:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
        case eDevState_PRESENT:
        {
            g_Dev_Type = eDevType_NONE;
            g_DevState = eDevState_DEPARTED;
        } break;
        case eDevState_DEPARTED:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
    }

    framework_UnlockMutex(g_devLock);

    framework_LockMutex(g_SnepClientLock);

    switch(g_SnepClientState)
    {
        case eSnepClientState_WAIT_OFF:
        {
            g_SnepClientState = eSnepClientState_OFF;
            framework_NotifyMutex(g_SnepClientLock, 0);
        } break;
        case eSnepClientState_OFF:
        {
        } break;
        case eSnepClientState_WAIT_READY:
        {
            g_SnepClientState = eSnepClientState_OFF;
            framework_NotifyMutex(g_SnepClientLock, 0);
        } break;
        case eSnepClientState_READY:
        {
            g_SnepClientState = eSnepClientState_OFF;
        } break;
        case eSnepClientState_EXIT:
        {
        } break;
    }

    framework_UnlockMutex(g_SnepClientLock);
}

void onMessageReceived(unsigned char *message, unsigned int length)
{
    unsigned int i = 0x00;
    qDebug() << "NDEF Message Received :";
    qDebug() << message << length;
    //PrintNDEFContent(NULL, NULL, message, length);
}

void onSnepClientReady()
{
    framework_LockMutex(g_devLock);

    switch(g_DevState)
    {
        case eDevState_WAIT_DEPARTURE:
        {
            g_DevState = eDevState_PRESENT;
            g_Dev_Type = eDevType_P2P;
            framework_NotifyMutex(g_devLock, 0);
        } break;
        case eDevState_EXIT:
        {
            g_Dev_Type = eDevType_P2P;
        } break;
        case eDevState_NONE:
        {
            g_DevState = eDevState_PRESENT;
            g_Dev_Type = eDevType_P2P;
        } break;
        case eDevState_WAIT_ARRIVAL:
        {
            g_DevState = eDevState_PRESENT;
            g_Dev_Type = eDevType_P2P;
            framework_NotifyMutex(g_devLock, 0);
        } break;
        case eDevState_PRESENT:
        {
            g_Dev_Type = eDevType_P2P;
        } break;
        case eDevState_DEPARTED:
        {
            g_Dev_Type = eDevType_P2P;
            g_DevState = eDevState_PRESENT;
        } break;
    }

    framework_UnlockMutex(g_devLock);

    framework_LockMutex(g_SnepClientLock);

    switch(g_SnepClientState)
    {
        case eSnepClientState_WAIT_OFF:
        {
            g_SnepClientState = eSnepClientState_READY;
            framework_NotifyMutex(g_SnepClientLock, 0);
        } break;
        case eSnepClientState_OFF:
        {
            g_SnepClientState = eSnepClientState_READY;
        } break;
        case eSnepClientState_WAIT_READY:
        {
            g_SnepClientState = eSnepClientState_READY;
            framework_NotifyMutex(g_SnepClientLock, 0);
        } break;
        case eSnepClientState_READY:
        {
        } break;
        case eSnepClientState_EXIT:
        {
        } break;
    }

    framework_UnlockMutex(g_SnepClientLock);
}

void onSnepClientClosed()
{
    framework_LockMutex(g_devLock);

    switch(g_DevState)
    {
        case eDevState_WAIT_DEPARTURE:
        {
            g_DevState = eDevState_DEPARTED;
            g_Dev_Type = eDevType_NONE;
            framework_NotifyMutex(g_devLock, 0);
        } break;
        case eDevState_EXIT:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
        case eDevState_NONE:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
        case eDevState_WAIT_ARRIVAL:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
        case eDevState_PRESENT:
        {
            g_Dev_Type = eDevType_NONE;
            g_DevState = eDevState_DEPARTED;
        } break;
        case eDevState_DEPARTED:
        {
            g_Dev_Type = eDevType_NONE;
        } break;
    }
    framework_UnlockMutex(g_devLock);

    framework_LockMutex(g_SnepClientLock);

    switch(g_SnepClientState)
    {
        case eSnepClientState_WAIT_OFF:
        {
            g_SnepClientState = eSnepClientState_OFF;
            framework_NotifyMutex(g_SnepClientLock, 0);
        } break;
        case eSnepClientState_OFF:
        {
        } break;
        case eSnepClientState_WAIT_READY:
        {
            g_SnepClientState = eSnepClientState_OFF;
            framework_NotifyMutex(g_SnepClientLock, 0);
        } break;
        case eSnepClientState_READY:
        {
            g_SnepClientState = eSnepClientState_OFF;
        } break;
        case eSnepClientState_EXIT:
        {
        } break;
    }

    framework_UnlockMutex(g_SnepClientLock);
}

//************************* Functions *******************************//

int nfc_pn7150::InitMode(int tag, int p2p, int hce)
{
    int res = 0x00;

    g_TagCB.onTagArrival = onTagArrival;
    g_TagCB.onTagDeparture = onTagDeparture;

    g_SnepServerCB.onDeviceArrival = onDeviceArrival;
    g_SnepServerCB.onDeviceDeparture = onDeviceDeparture;
    g_SnepServerCB.onMessageReceived = onMessageReceived;

    g_SnepClientCB.onDeviceArrival = onSnepClientReady;
    g_SnepClientCB.onDeviceDeparture = onSnepClientClosed;

    if(0x00 == res)
    {
        res = nfcManager_doInitialize();
        if(0x00 != res)
        {
            qDebug() << "NfcService Init Failed";
        }
    }

    if(0x00 == res)
    {
        if(0x01 == tag)
        {
            nfcManager_registerTagCallback(&g_TagCB);
        }

        if(0x01 == p2p)
        {
            res = nfcSnep_registerClientCallback(&g_SnepClientCB);
            if(0x00 != res)
            {
                qDebug() << "SNEP Client Register Callback Failed";
            }
        }
    }

    if(0x00 == res)
    {

        nfcManager_enableDiscovery(DEFAULT_NFA_TECH_MASK, 0x00, 0, 0);

        if(0x01 == p2p)
        {
            res = nfcSnep_startServer(&g_SnepServerCB);
            if(0x00 != res)
            {
                qDebug() << "Start SNEP Server Failed";
            }
        }
    }

    return res;
}

int nfc_pn7150::DeinitPollMode()
{
    int res = 0x00;

    nfcSnep_stopServer();

    nfcManager_disableDiscovery();

    nfcSnep_deregisterClientCallback();

    nfcManager_deregisterTagCallback();

    res = nfcManager_doDeinitialize();

    if(0x00 != res)
    {
        qDebug() << "NFC Service Deinit Failed";
    }
    return res;
}

int nfc_pn7150::SnepPush(unsigned char* msgToPush, unsigned int len)
{
    int res = 0x00;

    framework_LockMutex(g_devLock);
    framework_LockMutex(g_SnepClientLock);

    if(eSnepClientState_READY != g_SnepClientState && eSnepClientState_EXIT!= g_SnepClientState && eDevState_PRESENT == g_DevState)
    {
        framework_UnlockMutex(g_devLock);
        g_SnepClientState = eSnepClientState_WAIT_READY;
        framework_WaitMutex(g_SnepClientLock, 0);
    }
    else
    {
        framework_UnlockMutex(g_devLock);
    }

    if(eSnepClientState_READY == g_SnepClientState)
    {
        framework_UnlockMutex(g_SnepClientLock);
        res = nfcSnep_putMessage(msgToPush, len);

        if(0x00 != res)
        {
            qDebug() << "Push Failed";
        }
        else
        {
            qDebug() << "Push successful";
        }
    }
    else
    {
        framework_UnlockMutex(g_SnepClientLock);
    }

    return res;
}

/*mode = 1 => poll mode = 2 => push mode = 3 => ndef write 4 => HCE*/
int nfc_pn7150::WaitDeviceArrival(int mode, unsigned char* msgToSend, unsigned int len)
{
    int res = 0x00;
    unsigned int i = 0x00;
    int block = 0x01;
    unsigned char key[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ndef_info_t NDEFinfo;
    eDevType DevTypeBck = eDevType_NONE;
    unsigned char MifareAuthCmd[] = {0x60U, 0x00 /*block*/, 0x02, 0x02, 0x02, 0x02, 0x00 /*key*/, 0x00 /*key*/, 0x00 /*key*/, 0x00 /*key*/ , 0x00 /*key*/, 0x00 /*key*/};
    unsigned char MifareReadCmd[] = {0x30U,  /*block*/ 0x00};
    nfc_tag_info_t TagInfo;
    MifareAuthCmd[1] = block;
    memcpy(&MifareAuthCmd[6], key, 6);
    MifareReadCmd[1] = block;

    do
    {
        framework_LockMutex(g_devLock);
        if(eDevState_EXIT == g_DevState)
        {
            framework_UnlockMutex(g_devLock);
            break;
        }

        else if(eDevState_PRESENT != g_DevState)
        {
            qDebug() << "...Waiting for a Tag/Device... g_DevState";
            g_DevState = eDevState_WAIT_ARRIVAL;
            framework_WaitMutex(g_devLock, 0);
        }

        if(eDevState_EXIT == g_DevState)
        {
            framework_UnlockMutex(g_devLock);
            break;
        }

        if(eDevState_PRESENT == g_DevState)
        {
            DevTypeBck = g_Dev_Type;
            if(eDevType_TAG == g_Dev_Type)
            {
                memcpy(&TagInfo, &g_TagInfo, sizeof(nfc_tag_info_t));
                framework_UnlockMutex(g_devLock);
                res = nfcTag_isNdef(TagInfo.handle, &NDEFinfo);
                if(0x01 == res)
                {
                    qDebug() << "PrintNDEFContent(&TagInfo, &NDEFinfo, NULL, 0x00);";
                    PrintNDEFContent(&TagInfo, &NDEFinfo, NULL, 0x00);
                }
            }
            else if(eDevType_P2P == g_Dev_Type)/*P2P Detected*/
            {
                framework_UnlockMutex(g_devLock);
                qDebug() << "Device Found P2P";

                framework_LockMutex(g_SnepClientLock);

                if(eSnepClientState_READY == g_SnepClientState)
                {
                    g_SnepClientState = eSnepClientState_WAIT_OFF;
                    framework_WaitMutex(g_SnepClientLock, 0);
                }
                framework_UnlockMutex(g_SnepClientLock);
                framework_LockMutex(g_devLock);
            }
            else
            {
                framework_UnlockMutex(g_devLock);
                break;
            }

            if(eDevState_PRESENT == g_DevState)
            {
                g_DevState = eDevState_WAIT_DEPARTURE;
                framework_WaitMutex(g_devLock, 0);
                if(eDevType_P2P == DevTypeBck)
                {
                    qDebug() << "Device Lost P2P";
                }
                DevTypeBck = eDevType_NONE;
            }
            else if(eDevType_P2P == DevTypeBck)
            {
                qDebug() << "Device Lost";
            }
        }

        framework_UnlockMutex(g_devLock);
    }while(0x01);

    return res;
}

void nfc_pn7150::strtolower(char * string)
{
    unsigned int i = 0x00;

    for(i = 0; i < strlen(string); i++)
    {
        string[i] = tolower(string[i]);
    }
}

char* nfc_pn7150::strRemovceChar(const char* str, char car)
{
    unsigned int i = 0x00;
    unsigned int index = 0x00;
    char * dest = (char*)malloc((strlen(str) + 1) * sizeof(char));

    for(i = 0x00; i < strlen(str); i++)
    {
        if(str[i] != car)
        {
            dest[index++] = str[i];
        }
    }
    dest[index] = '\0';
    return dest;
}

int nfc_pn7150::convertParamtoBuffer(char* param, unsigned char** outBuffer, unsigned int* outBufferLen)
{
    int res = 0x00;
    unsigned int i = 0x00;
    int index = 0x00;
    char atoiBuf[3];
    atoiBuf[2] = '\0';

    if(NULL == param || NULL == outBuffer || NULL == outBufferLen)
    {
        qDebug() << "Parameter Error";
        res = 0xFF;
    }

    if(0x00 == res)
    {
        param = strRemovceChar(param, ' ');
    }

    if(0x00 == res)
    {
        if(0x00 == strlen(param) % 2)
        {
            *outBufferLen = strlen(param) / 2;

            *outBuffer = (unsigned char*) malloc((*outBufferLen) * sizeof(unsigned char));
            if(NULL != *outBuffer)
            {
                for(i = 0x00; i < ((*outBufferLen) * 2); i = i + 2)
                {
                    atoiBuf[0] = param[i];
                    atoiBuf[1] = param[i + 1];
                    (*outBuffer)[index++] = strtol(atoiBuf, NULL, 16);
                }
            }
            else
            {
                qDebug() << "Memory Allocation Failed";
                res = 0xFF;
            }
        }
        else
        {
            qDebug() << "Invalid NDEF Message Param";
        }
        free(param);
    }

    return res;
}

/*if data = NULL this tag is not followed by dataStr : for example -h --help
if format = 0 tag format -t "text" if format=1 tag format : --type=text*/
int nfc_pn7150::LookForTag(char** args, int args_len, char* tag, char** data, int format)
{
    int res = 0xFF;
    int i = 0x00;
    int found = 0xFF;

    for(i = 0x00; i < args_len; i++)
    {
        found = 0xFF;
        strtolower(args[i]);
        if(0x00 == format)
        {
            found = strcmp(args[i], tag);
        }
        else
        {
            found = strncmp(args[i], tag, strlen(tag));
        }

        if(0x00 == found)
        {
            if(NULL != data)
            {
                if(0x00 == format)
                {
                    if(i < (args_len - 1))
                    {
                        *data = args[i + 1];
                        res = 0x00;
                        break;
                    }
                    else
                    {
                        qDebug() << "Argument missing after " << tag;
                    }
                }
                else
                {
                    *data = &args[i][strlen(tag) + 1]; /* +1 to remove '='*/
                    res = 0x00;
                    break;
                }
            }
            else
            {
                res = 0x00;
                break;
            }
        }
    }

    return res;
}

void* ExitThread(void* pContext)
{
    while (true)
    {

    }

    framework_LockMutex(g_SnepClientLock);

    if(eSnepClientState_WAIT_OFF == g_SnepClientState || eSnepClientState_WAIT_READY == g_SnepClientState)
    {
        g_SnepClientState = eSnepClientState_EXIT;
        framework_NotifyMutex(g_SnepClientLock, 0);
    }
    else
    {
        g_SnepClientState = eSnepClientState_EXIT;
    }
    framework_UnlockMutex(g_SnepClientLock);

    framework_LockMutex(g_devLock);

    if(eDevState_WAIT_ARRIVAL == g_DevState || eDevState_WAIT_DEPARTURE == g_DevState)
    {
        g_DevState = eDevState_EXIT;
        framework_NotifyMutex(g_devLock, 0);
    }
    else
    {
        g_DevState = eDevState_EXIT;
    }

    framework_UnlockMutex(g_devLock);
    return NULL;
}

int nfc_pn7150::InitEnv()
{
    eResult tool_res = FRAMEWORK_SUCCESS;
    int res = 0x00;
    tool_res = framework_CreateMutex(&g_devLock);
    if(FRAMEWORK_SUCCESS != tool_res)
    {
        res = 0xFF;
    }
    if(0x00 == res)
    {
        tool_res = framework_CreateMutex(&g_SnepClientLock);
        if(FRAMEWORK_SUCCESS != tool_res)
        {
            res = 0xFF;
        }
    }
    if(0x00 == res)
    {
        tool_res = framework_CreateThread(&g_ThreadHandle, ExitThread, NULL);
        if(FRAMEWORK_SUCCESS != tool_res)
        {
            res = 0xFF;
        }
    }
    return res;
}

int nfc_pn7150::CleanEnv()
{
    if(NULL != g_ThreadHandle)
    {
        framework_DeleteThread(g_ThreadHandle);
        g_ThreadHandle = NULL;
    }

    if(NULL != g_devLock)
    {
        framework_DeleteMutex(g_devLock);
        g_devLock = NULL;
    }

    if(NULL != g_SnepClientLock)
    {
        framework_DeleteMutex(g_SnepClientLock);
        g_SnepClientLock = NULL;
    }
    return 0x00;
}

void nfc_pn7150::cmd_poll()
{
    int res = 0x00;
    InitEnv();
    res = InitMode(0x01, 0x01, 0x00);
    if(0x00 == res)
    {
        WaitDeviceArrival(0x01, NULL , 0x00);
    }
    res = DeinitPollMode();
    qDebug() << "Deinit" << res;
    res = CleanEnv();
    qDebug() << "clean" << res;
    qDebug() << "DONE";
}

void nfc_pn7150::PrintNDEFContent(nfc_tag_info_t* TagInfo, ndef_info_t* NDEFinfo, unsigned char* ndefRaw, unsigned int ndefRawLen)
{
    unsigned char* NDEFContent = NULL;
    nfc_friendly_type_t lNDEFType = NDEF_FRIENDLY_TYPE_OTHER;
    unsigned int res = 0x00;
    unsigned int i = 0x00;
    char* TextContent = NULL;
    char* URLContent = NULL;
    nfc_handover_select_t HandoverSelectContent;
    nfc_handover_request_t HandoverRequestContent;
    if(NULL != NDEFinfo)
    {
        ndefRawLen = NDEFinfo->current_ndef_length;
        NDEFContent = (unsigned char *)malloc(ndefRawLen * sizeof(unsigned char));
        res = nfcTag_readNdef(TagInfo->handle, NDEFContent, ndefRawLen, &lNDEFType);
    }
    else
    {
        qDebug() << "Error : Invalid Parameters";
    }

    if(res != ndefRawLen)
    {
        qDebug() << "Read NDEF Content Failed";
    }
    else
    {
        switch(lNDEFType)
        {
            case NDEF_FRIENDLY_TYPE_TEXT:
            {
                qDebug() << "NDEF_FRIENDLY_TYPE_TEXT";
                TextContent = (char * )malloc(res * sizeof(char));
                res = ndef_readText(NDEFContent, res, TextContent, res);
                if(0x00 == res)
                {
                    qDebug() << "Type : 'Text'";
                    qDebug() << TextContent;
                    QString dataUpper = TextContent;
                    dataUpper = dataUpper.toUpper();
                    qDebug() << dataUpper.toUpper();
                    emit sigCardData(dataUpper);

                }
                else
                {
                    qDebug() << "Read NDEF Text Error";
                }
                if(NULL != TextContent)
                {
                    free(TextContent);
                    TextContent = NULL;
                }
            } break;
            case NDEF_FRIENDLY_TYPE_URL:
            {
                qDebug() << "NDEF_FRIENDLY_TYPE_URL";
                /*NOTE : + 27 = Max prefix lenght*/
                URLContent = (char * )malloc(res * sizeof(unsigned char) + 27 );
                memset(URLContent, 0x00, res * sizeof(unsigned char) + 27);
                res = ndef_readUrl(NDEFContent, res, URLContent, res + 27);
                if(0x00 == res)
                {
                    qDebug() << "Type : 'URI'";
                    qDebug() << URLContent;
                    /*NOTE: open url in browser*/
                    /*open_uri(URLContent);*/
                }
                else
                {
                    qDebug() << "Read NDEF URL Error";
                }
                if(NULL != URLContent)
                {
                    free(URLContent);
                    URLContent = NULL;
                }
            } break;
            case NDEF_FRIENDLY_TYPE_HS:
            {
                qDebug() << "NDEF_FRIENDLY_TYPE_HS";
                res = ndef_readHandoverSelectInfo(NDEFContent, res, &HandoverSelectContent);
                if(0x00 == res)
                {
                    qDebug() << "Handover Select : ";

                    qDebug() << "Bluetooth : Power state : ";
                    switch(HandoverSelectContent.bluetooth.power_state)
                    {
                        case HANDOVER_CPS_INACTIVE:
                        {
                            qDebug() << "'Inactive'";
                        } break;
                        case HANDOVER_CPS_ACTIVE:
                        {
                            qDebug() << "'Active'";
                        } break;
                        case HANDOVER_CPS_ACTIVATING:
                        {
                            qDebug() << "'Activating'";
                        } break;
                        case HANDOVER_CPS_UNKNOWN:
                        {
                            qDebug() << "'Unknown'";
                        } break;
                        default:
                        {
                            qDebug() << "'Unknown'";
                        } break;
                    }
                    if(HANDOVER_TYPE_BT == HandoverSelectContent.bluetooth.type)
                    {
                        qDebug() << "Type : 'BT'";
                    }
                    else if(HANDOVER_TYPE_BLE == HandoverSelectContent.bluetooth.type)
                    {
                        qDebug() << "Type : 'BLE'";
                    }
                    else
                    {
                        qDebug() << "Type : 'Unknown'";
                    }
                    qDebug() << "Address : '";
                    for(i = 0x00; i < 6; i++)
                    {
                        qDebug() << HandoverSelectContent.bluetooth.address[i];
                    }
                    qDebug() << "'Device Name : '";
                    for(i = 0x00; i < HandoverSelectContent.bluetooth.device_name_length; i++)
                    {
                        qDebug() << HandoverSelectContent.bluetooth.device_name[i];
                    }
                    qDebug() << "'NDEF Record : ";
                    for(i = 0x01; i < HandoverSelectContent.bluetooth.ndef_length+1; i++)
                    {
                        qDebug() << HandoverSelectContent.bluetooth.ndef[i];
                        if(i%8 == 0)
                        {
                            qDebug() << "";
                        }
                    }
                    qDebug() << "WIFI : Power state : ";
                    switch(HandoverSelectContent.wifi.power_state)
                    {
                        case HANDOVER_CPS_INACTIVE:
                        {
                            qDebug() << " 'Inactive'";
                        } break;
                        case HANDOVER_CPS_ACTIVE:
                        {
                            qDebug() << " 'Active'";
                        } break;
                        case HANDOVER_CPS_ACTIVATING:
                        {
                            qDebug() << " 'Activating'";
                        } break;
                        case HANDOVER_CPS_UNKNOWN:
                        {
                            qDebug() << " 'Unknown'";
                        } break;
                        default:
                        {
                            qDebug() << " 'Unknown'";
                        } break;
                    }

                    qDebug() << "SSID : '";
                    for(i = 0x01; i < HandoverSelectContent.wifi.ssid_length+1; i++)
                    {
                        qDebug() << HandoverSelectContent.wifi.ssid[i];
                        if(i%30 == 0)
                        {
                            qDebug() << "";
                        }
                    }
                    qDebug() << "'Key : '";
                    for(i = 0x01; i < HandoverSelectContent.wifi.key_length+1; i++)
                    {
                        qDebug() << HandoverSelectContent.wifi.key[i];
                        if(i%30 == 0)
                        {
                            qDebug() << "";
                        }
                    }
                    qDebug() << "'NDEF Record : ";
                    for(i = 0x01; i < HandoverSelectContent.wifi.ndef_length+1; i++)
                    {
                        qDebug() << HandoverSelectContent.wifi.ndef[i];
                        if(i%30 == 0)
                        {
                            qDebug() << "";
                        }
                    }
                    qDebug() << "";
                }
                else
                {
                    qDebug() << "Read NDEF Handover Select Failed\n";
                }

            } break;
            default:
            {
            } break;
        }
        //qDebug() << "NDEF data received of Length " + QString::number(ndefRawLen);
        QByteArray data;
        for(i = 0x00; i < ndefRawLen; i++)
        {
            data.append(NDEFContent[i]);
            if(i%30 == 0 && 0x00 != i)
            {
                //qDebug() << "";
            }
        }
        data = data.toHex().toUpper();
        //qDebug() << data;
        //emit sigCardData(data);
    }

    if(NULL != NDEFContent)
    {
        free(NDEFContent);
        NDEFContent = NULL;
    }
}

void nfc_pn7150::sltNfcEndThread()
{
    nfcThread = false;
}
