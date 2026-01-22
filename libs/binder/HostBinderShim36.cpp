#include <binder/HostBinderShim36.h>        
#include <binder/IBinder.h>
#include <binder/Parcel.h>

namespace android {
    void HostBinderShim36::writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> binder){
        // place of length
	int length_pos = data.dataPosition();
	data.writeInt32(0);
	// magic number
	data.writeInt32(0x4C444E42);
        int prev = data.dataPosition();
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
	data.setDataPosition(length_pos);
	// write length here
	data.writeInt32(cur - prev);
	ALOGE("bundle size: %d", cur - prev);
	data.setDataPosition(cur);
	// API 36: mHasIntent
	data.writeInt32(0);
	// API 35: no original intent
	data.writeInt32(0);
	// API 35: preventIntentRedirect
	data.writeInt32(0);
    }

    void HostBinderShim36::writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle) {
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
	// API 35: mExtendedFlags
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
	    out.writeInt32(0);
	    // API 35: no original intent
	    out.writeInt32(0);
	    // API 35: preventIntentRedirect
	    out.writeInt32(0);
	}
    }

    void HostBinderShim36::broadCastIntent(Parcel &data, const char* key, sp<IBinder> binder) {
        writeInterfaceToken(data, String16("android.app.IActivityManager"));
	// arg0
        data.writeStrongBinder(NULL);
	// finishFlatten
	data.writeInt32(0);
	ALOGD("interface size: %zu", data.dataSize());
	// arg1
	writeIntent(data, "com.github.ananbox", "com.github.ananbox.BinderReceiver", true);
	ALOGD("writeintent size: %zu", data.dataSize());
	writeBroadcastBundle(data, key, binder);
	ALOGD("broadcastBundle size: %zu", data.dataSize());
	// arg2
	data.writeString16(NULL, -1);
	// arg3
	data.writeStrongBinder(NULL);
	// finishFlatten
	data.writeInt32(0);
	// arg4
	data.writeInt32(0);
	// arg5
	data.writeString16(NULL, -1);
	ALOGD("null string size: %zu", data.dataSize());
	// arg6: null bundle
	data.writeInt32(-1);
	ALOGD("null bundle size: %zu", data.dataSize());
	// arg7: null string array
	data.writeInt32(-1);
	ALOGD("null string array size: %zu", data.dataSize());
	// arg8
	data.writeInt32(0);
	// arg9: null bundle
	data.writeInt32(-1);
	// arg10
	data.writeBool(true);
	// arg11
	data.writeBool(false);
	// arg12
	data.writeInt32(0);
    }

    void HostBinderShim36::finishFlattenBinder(Parcel &data, sp<IBinder> binder) {
        data.writeInt32(binder == NULL ? 0 : 0b001100);
    }
    
    void HostBinderShim36::sendBroadCast(sp<IBinder> ams, Parcel &data) {
        ams->transact(22, data, NULL, 0);
    }
}
