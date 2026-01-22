#ifndef __HOSTBINDERSHIM36_H__
#define __HOSTBINDERSHIM36_H__
#include <binder/HostBinderShim30.h>
namespace android {
    class HostBinderShim36: public HostBinderShim30 {
        public:
            void broadCastIntent(Parcel &data, const char* key, sp<IBinder> binder);
            void writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> binder);
            void writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle);
            void finishFlattenBinder(Parcel &data, sp<IBinder> binder);
            void sendBroadCast(sp<IBinder> ams, Parcel &data);
    };
}
#endif
