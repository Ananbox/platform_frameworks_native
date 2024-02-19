#include <binder/HostBinderShim31.h>        
#include <binder/IBinder.h>
#include <binder/Parcel.h>

namespace android {
    void HostBinderShim31::finishFlattenBinder(Parcel &data, sp<IBinder> binder) {
        Category category;
        category.version = 1;
        if (binder == NULL) {
            category.level = 0;
        }
        else {
            category.level = 0b001100;
        }
        data.writeInt32(*((uint32_t *) &category));
    }
    
    void HostBinderShim31::sendBroadCast(sp<IBinder> ams, Parcel &data) {
        ams->transact(15, data, NULL, 0);
    }
}
