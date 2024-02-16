#include <binder/HostBinderShim.h> 
#include <binder/IBinder.h>
#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>

#include <binder/HostBinder.h>
#include <binder/HostBinderShim.h>
#include <binder/HostBinderShim30.h>

namespace android {

LocalBinder::LocalBinder(std::shared_ptr<HostBinderShim> &shim_): remoteBinder(NULL) {
    shim = shim_;
}

status_t LocalBinder::onTransact(
        uint32_t code, const Parcel& data, Parcel* /*reply*/, uint32_t /*flags*/)
{
    switch (code) {
        case 1:
            //getPid()
            return NO_ERROR;
        case 2:
            //receiveRemoteBinder
            ALOGD("onReceiveBinder()\n");
            // enforce descriptor
            shim->enforceDescriptor(data);

            remoteBinder = data.readStrongBinder();

            if (remoteBinder != NULL) {
                ALOGD("get service binder success\n");
            }
            else {
                ALOGE("get service binder failed\n");
            }
            return NO_ERROR;
        default:
            return NO_ERROR;
    }
}

std::shared_ptr<HostBinderShim> HostBinder::getShim() {
    return std::make_shared<HostBinderShim30>();
}

HostBinder::HostBinder(int mDriverFD_): mDriverFD(mDriverFD_), shim(getShim()) {
}

#define BINDER_VM_SIZE ((1*1024*1024) - (4096 *2))
HostBinder::HostBinder(binder_state *bs): shim(getShim()) {
    IPCThreadState* ipc = IPCThreadState::self();
    bs->fd = ipc->mProcess->mDriverFD;
    bs->mapped = ipc->mProcess->mVMStart;
    bs->mapsize = BINDER_VM_SIZE;
    mDriverFD = bs->fd;
}

sp<IBinder> HostBinder::getSVMObj() {
    Parcel data;
    sp<IBinder> object(NULL);
    IPCThreadState* ipc = IPCThreadState::self();
    sp<IBinder> ams(shim->getHostAMS());
    sp<LocalBinder> local = new LocalBinder(shim);
    shim->broadCastIntent(data, "local", local);
    shim->sendBroadCast(ams, data);
    ALOGD("wait for service binder\n");
    ipc->setupPolling(&mDriverFD);
    ipc->handlePolledCommands();
    LOG_ALWAYS_FATAL_IF(local->remoteBinder == NULL, "get service binder failed.  Terminating.");
    ALOGD("get service binder\n");
    object = local->remoteBinder;
    return object;
}

void HostBinder::publishSVM() {
    sp<IBinder> ams(shim->getHostAMS());
    sp<LocalBinder> binder = new LocalBinder(shim);
    Parcel data;
    shim->broadCastIntent(data, "binder", binder);
    shim->sendBroadCast(ams, data);
}

}
