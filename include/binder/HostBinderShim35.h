#ifndef __HOSTBINDERSHIM35_H__
#define __HOSTBINDERSHIM35_H__
#include <binder/HostBinderShim30.h>
namespace android {
    class HostBinderShim35: public HostBinderShim30 {
        public:
            void writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> binder);
            void writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle);
            void finishFlattenBinder(Parcel &data, sp<IBinder> binder);
            void sendBroadCast(sp<IBinder> ams, Parcel &data);
    };
}
#endif
