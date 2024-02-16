#ifndef __HOSTBINDER_H__
#define __HOSTBINDER_H__
#include <utils/RefBase.h>
#include <binder/HostBinderShim.h>
struct binder_state
{
    int fd;
    void *mapped;
    size_t mapsize;
};
namespace android {
    class IBinder;
    class HostBinder {
        public:
            HostBinder(int mDriverFD_);
            HostBinder(binder_state* bs);
            sp<IBinder> getSVMObj();
            void publishSVM();
        private:
            int mDriverFD;
            std::shared_ptr<HostBinderShim> shim;
            std::shared_ptr<HostBinderShim> getShim();
    };
    class LocalBinder : public BBinder
    {
        public:
            sp<IBinder> remoteBinder;
            LocalBinder(std::shared_ptr<HostBinderShim> &shim_);
        protected:
            virtual status_t    onTransact( uint32_t code,
                    const Parcel& data,
                    Parcel* reply,
                    uint32_t flags = 0);
        private:
            std::shared_ptr<HostBinderShim> shim;
    };
}
#endif
