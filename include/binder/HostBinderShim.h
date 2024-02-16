#ifndef __HOSTBINDERSHIM_H__
#define __HOSTBINDERSHIM_H__
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <binder/IBinder.h>
#include <binder/Parcel.h>
namespace android {
    class Parcel;
    class IBinder;
    class HostBinderShim {
        public:
            sp<IBinder> getHostAMS(); 
            virtual void writeInterfaceToken(Parcel &data, String16 name) = 0;
            virtual void writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle) = 0;
            virtual void writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> binder) = 0;
            virtual void broadCastIntent(Parcel &data, const char* key, sp<IBinder> binder) = 0;
            virtual void sendBroadCast(sp<IBinder> ams, Parcel &data) = 0;
            virtual void enforceDescriptor(const Parcel &data) = 0;
            virtual bool needReadStatus() = 0;
    };
}
#endif
