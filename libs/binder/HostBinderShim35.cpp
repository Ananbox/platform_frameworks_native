#include <binder/HostBinderShim35.h>        
#include <binder/IBinder.h>
#include <binder/Parcel.h>

namespace android {
    void HostBinderShim35::writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> binder){
	HostBinderShim30::writeBroadcastBundle(data, key, binder);
	// no original intent
	data.writeInt32(0);
    }

    void HostBinderShim35::writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle) {
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
	// mExtendedFlags
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
	if (!hasBundle) {
	    out.writeInt32(-1);
	    out.writeInt32(0);
	}
    }

    void HostBinderShim35::finishFlattenBinder(Parcel &data, sp<IBinder> binder) {
        data.writeInt32(binder == NULL ? 0 : 0b001100);
    }
    
    void HostBinderShim35::sendBroadCast(sp<IBinder> ams, Parcel &data) {
        ams->transact(22, data, NULL, 0);
    }
}
