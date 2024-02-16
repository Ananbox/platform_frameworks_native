#ifndef __HOSTBINDERSHIM30_H__
#define __HOSTBINDERSHIM30_H__
#include <binder/HostBinderShim.h>
namespace android {
    class HostBinderShim30: public HostBinderShim {
        public:
            void writeInterfaceToken(Parcel &data, String16 name);
            void writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle);
            void writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> binder);
            void broadCastIntent(Parcel &data, const char* key, sp<IBinder> binder);
            void sendBroadCast(sp<IBinder> ams, Parcel &data);
            void enforceDescriptor(const Parcel &data);
            bool needReadStatus();
    };
}
#endif
