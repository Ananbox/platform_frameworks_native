/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "ProcessState"

#include <cutils/process_name.h>

#include <binder/ProcessState.h>

#include <utils/Atomic.h>
#include <binder/BpBinder.h>
#include <binder/IPCThreadState.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <binder/IServiceManager.h>
#include <utils/String8.h>
#include <utils/threads.h>

#include <private/binder/binder_module.h>
#include <private/binder/Static.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

//#include <ananbox.h>

#define BINDER_VM_SIZE ((1*1024*1024) - (4096 *2))
#define DEFAULT_MAX_BINDER_THREADS 15

// -------------------------------------------------------------------------

namespace android {

class PoolThread : public Thread
{
public:
    PoolThread(bool isMain)
        : mIsMain(isMain)
    {
    }
    
protected:
    virtual bool threadLoop()
    {
        IPCThreadState::self()->joinThreadPool(mIsMain);
        return false;
    }
    
    const bool mIsMain;
};

sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }
    gProcess = new ProcessState;
    return gProcess;
}

void ProcessState::setContextObject(const sp<IBinder>& object)
{
    setContextObject(object, String16("default"));
}

#define STRICT_MODE_PENALTY_GATHER (0x40 << 16)
void writeInterfaceToken(Parcel &data, String16 name) {
    data.writeInt32(STRICT_MODE_PENALTY_GATHER);
    data.writeInt32(getuid());
    data.writeInt32(0x53595354);
    data.writeString16(name);
}

sp<IBinder> getHostAMS() {
    sp<IBinder> object(NULL);
    IPCThreadState* ipc = IPCThreadState::self();
    {
        Parcel data, reply;
        //data.writeInterfaceToken(String16("android.os.IServiceManager"));
        writeInterfaceToken(data, String16("android.os.IServiceManager"));
        data.writeString16(String16("activity"));
        status_t result = ipc->transact(0 /*magic*/, 1, data, &reply, 0);
        if (result == NO_ERROR) {
            // android 11: status
            reply.readInt32();
            object = reply.readStrongBinder();
        }
    }

    ipc->flushCommands();
    if (object == NULL) {
        ALOGE("get host AMS failed\n");
    }
    return object;
}

void writeIntent(Parcel &out, const char *mPackage, const char *mClass, bool hasBundle) {
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

void writeBroadcastBundle(Parcel &data, const char *key, sp<IBinder> local) {
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
    data.writeStrongBinder(local);
    // finishFlatten
    data.writeInt32(0b001100);
    int cur = data.dataPosition();
    // // go back & write length
    data.setDataPosition(prev);
    // write length here
    data.writeInt32(cur - prev - 4);
    data.setDataPosition(cur);
}

void broadCastIntent(sp<IBinder>& ams, sp<IBinder> local) {
    Parcel data;
    writeInterfaceToken(data, String16("android.app.IActivityManager"));
    data.writeStrongBinder(NULL);
    // finishFlatten
    data.writeInt32(0);
    writeIntent(data, "com.github.ananbox", "com.github.ananbox.BinderReceiver", true);
    writeBroadcastBundle(data, "local", local);
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
    ams->transact(14, data, NULL, 0);
}

class LocalBinder : public BBinder
{
    public:
        sp<IBinder> remoteBinder;
        LocalBinder(): remoteBinder(NULL) {}
    protected:
        virtual status_t    onTransact( uint32_t code,
                const Parcel& data,
                Parcel* reply,
                uint32_t flags = 0);
};

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
            data.readInt32();
            data.readInt32();
            data.readInt32();
            data.readString16();

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

sp<IBinder> ProcessState::getSVMObj() {
    sp<IBinder> object(NULL);
    IPCThreadState* ipc = IPCThreadState::self();
#if 0
    getSVM(mDriverFD);
    uint32_t handle = getSVMHandle();
#endif
    sp<IBinder> ams(getHostAMS());
    sp<LocalBinder> local = new LocalBinder();
    broadCastIntent(ams, local);
    ALOGD("wait for service binder\n");
#if 0
    ProcessState::self()->startThreadPool();
    while (local->remoteBinder == NULL); 
    ALOGD("get service binder\n");
#endif
    ipc->setupPolling(&mDriverFD);
    ipc->handlePolledCommands();
    LOG_ALWAYS_FATAL_IF(local->remoteBinder == NULL, "get service binder failed.  Terminating.");
    ALOGD("get service binder\n");
    object = local->remoteBinder;
    return object;
}

sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getSVMObj();
}

void ProcessState::setContextObject(const sp<IBinder>& object, const String16& name)
{
    AutoMutex _l(mLock);
    mContexts.add(name, object);
}

sp<IBinder> ProcessState::getContextObject(const String16& name, const sp<IBinder>& /*caller*/)
{
    mLock.lock();
    sp<IBinder> object(
        mContexts.indexOfKey(name) >= 0 ? mContexts.valueFor(name) : NULL);
    mLock.unlock();
    
    //printf("Getting context object %s for %p\n", String8(name).string(), caller.get());
    
    if (object != NULL) return object;

    // Don't attempt to retrieve contexts if we manage them
    if (mManagesContexts) {
        ALOGE("getContextObject(%s) failed, but we manage the contexts!\n",
            String8(name).string());
        return NULL;
    }
    
    // ananbox
#if 0
    IPCThreadState* ipc = IPCThreadState::self();
    {
        Parcel data, reply;
        // no interface token on this magic transaction
        data.writeString16(name);
        data.writeStrongBinder(caller);
        status_t result = ipc->transact(0 /*magic*/, 0, data, &reply, 0);
        if (result == NO_ERROR) {
            object = reply.readStrongBinder();
        }
    }

    ipc->flushCommands();
#endif

    object = getSVMObj();
    
    if (object != NULL) setContextObject(object, name);
    return object;
}

void ProcessState::startThreadPool()
{
    AutoMutex _l(mLock);
    if (!mThreadPoolStarted) {
        mThreadPoolStarted = true;
        spawnPooledThread(true);
    }
}

bool ProcessState::isContextManager(void) const
{
    return mManagesContexts;
}

bool ProcessState::becomeContextManager(context_check_func checkFunc, void* userData)
{
    if (!mManagesContexts) {
        AutoMutex _l(mLock);
        mBinderContextCheckFunc = checkFunc;
        mBinderContextUserData = userData;

        int dummy = 0;
        status_t result = ioctl(mDriverFD, BINDER_SET_CONTEXT_MGR, &dummy);
        if (result == 0) {
            mManagesContexts = true;
        } else if (result == -1) {
            mBinderContextCheckFunc = NULL;
            mBinderContextUserData = NULL;
            ALOGE("Binder ioctl to become context manager failed: %s\n", strerror(errno));
        }
    }
    return mManagesContexts;
}

ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size();
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = NULL;
        e.refs = NULL;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return NULL;
    }
    return &mHandleToObject.editItemAt(handle);
}

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        // We need to create a new BpBinder if there isn't currently one, OR we
        // are unable to acquire a weak reference on this current one.  See comment
        // in getWeakProxyForHandle() for more info about this.
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {
                // Special case for context manager...
                // The context manager is the only object for which we create
                // a BpBinder proxy without already holding a reference.
                // Perform a dummy transaction to ensure the context manager
                // is registered before we create the first local reference
                // to it (which will occur when creating the BpBinder).
                // If a local reference is created for the BpBinder when the
                // context manager is not present, the driver will fail to
                // provide a reference to the context manager, but the
                // driver API does not return status.
                //
                // Note that this is not race-free if the context manager
                // dies while this code runs.
                //
                // TODO: add a driver API to wait for context manager, or
                // stop special casing handle 0 for context manager and add
                // a driver API to get a handle to the context manager with
                // proper reference counting.

                Parcel data;
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, NULL, 0);
                if (status == DEAD_OBJECT)
                   return NULL;
            }

            b = new BpBinder(handle); 
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            // This little bit of nastyness is to allow us to add a primary
            // reference to the remote proxy when this team doesn't have one
            // but another team is sending the handle to us.
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}

wp<IBinder> ProcessState::getWeakProxyForHandle(int32_t handle)
{
    wp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {        
        // We need to create a new BpBinder if there isn't currently one, OR we
        // are unable to acquire a weak reference on this current one.  The
        // attemptIncWeak() is safe because we know the BpBinder destructor will always
        // call expungeHandle(), which acquires the same lock we are holding now.
        // We need to do this because there is a race condition between someone
        // releasing a reference on this BpBinder, and a new reference on its handle
        // arriving from the driver.
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            b = new BpBinder(handle);
            result = b;
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
        } else {
            result = b;
            e->refs->decWeak(this);
        }
    }

    return result;
}

void ProcessState::expungeHandle(int32_t handle, IBinder* binder)
{
    AutoMutex _l(mLock);
    
    handle_entry* e = lookupHandleLocked(handle);

    // This handle may have already been replaced with a new BpBinder
    // (if someone failed the AttemptIncWeak() above); we don't want
    // to overwrite it.
    if (e && e->binder == binder) e->binder = NULL;
}

String8 ProcessState::makeBinderThreadName() {
    int32_t s = android_atomic_add(1, &mThreadPoolSeq);
    pid_t pid = getpid();
    String8 name;
    name.appendFormat("Binder:%d_%X", pid, s);
    return name;
}

void ProcessState::spawnPooledThread(bool isMain)
{
    if (mThreadPoolStarted) {
        String8 name = makeBinderThreadName();
        ALOGV("Spawning new pooled thread, name=%s\n", name.string());
        sp<Thread> t = new PoolThread(isMain);
        t->run(name.string());
    }
}

status_t ProcessState::setThreadPoolMaxThreadCount(size_t maxThreads) {
    status_t result = NO_ERROR;
    if (ioctl(mDriverFD, BINDER_SET_MAX_THREADS, &maxThreads) != -1) {
        mMaxThreads = maxThreads;
    } else {
        result = -errno;
        ALOGE("Binder ioctl to set max threads failed: %s", strerror(-result));
    }
    return result;
}

void ProcessState::giveThreadPoolName() {
    androidSetThreadName( makeBinderThreadName().string() );
}

static int open_driver()
{
    int fd = open("/dev/binder", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        int vers = 0;
        status_t result = ioctl(fd, BINDER_VERSION, &vers);
        if (result == -1) {
            ALOGE("Binder ioctl to obtain version failed: %s", strerror(errno));
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
            ALOGE("Binder driver protocol does not match user space protocol!");
            close(fd);
            fd = -1;
        }
        size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
        if (result == -1) {
            ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
        }
    } else {
        ALOGW("Opening '/dev/binder' failed: %s\n", strerror(errno));
    }
    return fd;
}

ProcessState::ProcessState()
    : mDriverFD(open_driver())
    , mVMStart(MAP_FAILED)
    , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)
    , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)
    , mExecutingThreadsCount(0)
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)
    , mStarvationStartTimeMs(0)
    , mManagesContexts(false)
    , mBinderContextCheckFunc(NULL)
    , mBinderContextUserData(NULL)
    , mThreadPoolStarted(false)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // *sigh*
            ALOGE("Using /dev/binder failed: unable to mmap transaction memory.\n");
            close(mDriverFD);
            mDriverFD = -1;
        }
    }

    LOG_ALWAYS_FATAL_IF(mDriverFD < 0, "Binder driver could not be opened.  Terminating.");
}

ProcessState::~ProcessState()
{
}
        
}; // namespace android
