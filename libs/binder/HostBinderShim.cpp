#include <binder/HostBinderShim.h> 
#include <binder/IBinder.h>
#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>

#include <binder/HostBinderShim30.h>

#include <cstdio>
#include <fcntl.h>

namespace android {

sp<IBinder> HostBinderShim::getHostAMS() {
    sp<IBinder> object(NULL);
    IPCThreadState* ipc = IPCThreadState::self();
    {
        Parcel data, reply;
        //data.writeInterfaceToken(String16("android.os.IServiceManager"));
        writeInterfaceToken(data, String16("android.os.IServiceManager"));
        data.writeString16(String16("activity"));
        status_t result = ipc->transact(0 /*magic*/, 1, data, &reply, 0);
        if (result == NO_ERROR) {
            // API 30: read status
            if (needReadStatus())
                reply.readInt32();
            object = reply.readStrongBinder();
        }
    }

    ipc->flushCommands();
    if (object == NULL) {
        ALOGE("get host AMS failed\n");
    }
    return object;
}

void freeBuffer(Parcel* /*parcel*/, const uint8_t* data,
                                size_t /*dataSize*/,
                                const binder_size_t* /*objects*/,
                                size_t /*objectsSize*/, void* /*cookie*/)
{
    delete data;
}

void HostBinderShim::broadCastIntent(Parcel &data, const char* key, sp<IBinder> binder) {
    char path[255];
    snprintf(path, 255, "/%sBroadcastIntent", key);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        ALOGE("open broadcastIntent failed, err: %d", errno);
        return;
    }
    uint64_t sizes[2];
    if (read(fd, sizes, sizeof(sizes)) < 0) {
        ALOGE("read file failed");
        return;
    }
    uint64_t length = sizes[0] + sizes[1] * sizeof(uint64_t);
    uint8_t *ipcData = new uint8_t[length];
    if (read(fd, ipcData, sizeof(uint8_t) * length) != (ssize_t)length) {
        ALOGE("read data from file failed");
        return;
    }
    close(fd);
    const binder_size_t *ipcObjects = (binder_size_t *)&ipcData[sizes[0]];
    data.ipcSetDataReference(ipcData, sizes[0], ipcObjects, sizes[1], freeBuffer, nullptr);

    IBinder *local = binder->localBinder();
    for (uint64_t i = 0; i < sizes[1]; i++) {
        flat_binder_object* obj = (flat_binder_object *)&ipcData[ipcObjects[i]];
	if (obj->binder == 0 && obj->cookie == 0) {
	    continue;
	}
	obj->binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
	obj->cookie = reinterpret_cast<uintptr_t>(local);
    }
}

}
