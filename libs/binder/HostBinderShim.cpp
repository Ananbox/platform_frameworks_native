#include <binder/HostBinderShim.h> 
#include <binder/IBinder.h>
#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>

#include <binder/HostBinderShim30.h>

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

}
