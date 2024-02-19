#ifndef __HOSTBINDERSHIM31_H__
#define __HOSTBINDERSHIM31_H__
#include <binder/HostBinderShim30.h>
namespace android {
    class HostBinderShim31: public HostBinderShim30 {
        public:
            void finishFlattenBinder(Parcel &data, sp<IBinder> binder);
            void sendBroadCast(sp<IBinder> ams, Parcel &data);
    };
    // define struct to avoid byte-order related problems
    struct Category {
        uint8_t version;
        uint8_t reserved[2];
        uint8_t level;
    };
}
#endif
