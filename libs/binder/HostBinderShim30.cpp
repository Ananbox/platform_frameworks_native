#include <binder/HostBinderShim30.h>        
#include <binder/IBinder.h>
#include <binder/Parcel.h>

namespace android {

bool HostBinderShim30::needReadStatus() {
    return true;
}

#define STRICT_MODE_PENALTY_GATHER (0x40 << 16)
void HostBinderShim30::writeInterfaceToken(Parcel &data, String16 name) {
    data.writeInt32(STRICT_MODE_PENALTY_GATHER);
    data.writeInt32(getuid());
    data.writeInt32(0x53595354);
    data.writeString16(name);
}

void HostBinderShim30::writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle) {
    // indicate that there is an intent
    out.writeInt32(1);
    // mAction
    out.writeString16(NULL, -1);
    // uri mData
    out.writeInt32(0);
    // mType
    out.writeString16(NULL, -1);
    // mIdentifier
    out.writeString16(NULL, -1);
    // mFlags
    out.writeInt32(0);
    // mPackage
    out.writeString16(NULL, -1);
    // mComponent
    out.writeString16(String16(mPackage));
    out.writeString16(String16(mClass));
    // mSourceBounds
    out.writeInt32(0);
    // mCategories
    out.writeInt32(0);
    // mSelector
    out.writeInt32(0);
    // mClipData
    out.writeInt32(0);
    // mContentUserHint
    out.writeInt32(0);
    // mExtras
    if (!hasBundle)
        out.writeInt32(-1);
}

void HostBinderShim30::writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> binder) {
    // place of length
    int prev = data.dataPosition();
    data.writeInt32(0);
    // magic number
    data.writeInt32(0x4C444E42);
    // element count
    data.writeInt32(1);
    // Map
    // first binder element
    // key: binder
    data.writeString16(String16(key));
    // write type code: VAL_IBINDER
    data.writeInt32(15);
    data.writeStrongBinder(binder);
    // finishFlatten: stability
    finishFlattenBinder(data, binder);
    int cur = data.dataPosition();
    // // go back & write length
    data.setDataPosition(prev);
    // write length here
    data.writeInt32(cur - prev - 4);
    data.setDataPosition(cur);
}

void HostBinderShim30::finishFlattenBinder(Parcel &data, sp<IBinder> binder) {
    if (binder == NULL) {
        data.writeInt32(0);
    } else {
        data.writeInt32(0b001100);
    }
}

void HostBinderShim30::broadCastIntent(Parcel &data, const char* key, sp<IBinder> binder) {
    writeInterfaceToken(data, String16("android.app.IActivityManager"));
    data.writeStrongBinder(NULL);
    // finishFlatten
    data.writeInt32(0);
    writeIntent(data, "com.github.ananbox", "com.github.ananbox.BinderReceiver", true);
    writeBroadcastBundle(data, key, binder);
    data.writeString16(NULL, -1);
    data.writeStrongBinder(NULL);
    // finishFlatten
    data.writeInt32(0);
    data.writeInt32(0);
    data.writeString16(NULL, -1);
    // null bundle
    data.writeInt32(0);
    // null string array
    data.writeInt32(-1);
    data.writeInt32(-1);
    data.writeInt32(-1);
    // null bundle
    data.writeInt32(0);
    data.writeBool(true);
    data.writeBool(false);
    data.writeInt32(0);
}

void HostBinderShim30::sendBroadCast(sp<IBinder> ams, Parcel &data) {
    ams->transact(14, data, NULL, 0);
}

void HostBinderShim30::enforceDescriptor(const Parcel &data) {
    data.readInt32();
    data.readInt32();
    data.readInt32();
    data.readString16();
}

}
