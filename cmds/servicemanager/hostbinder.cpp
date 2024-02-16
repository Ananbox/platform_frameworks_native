#include <binder/HostBinder.h>
extern "C"
void publishSVM(binder_state *bs) {
    android::HostBinder host(bs);
    host.publishSVM();
}
