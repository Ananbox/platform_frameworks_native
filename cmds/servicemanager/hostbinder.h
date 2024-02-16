#ifndef __SVCHOSTBINDER_H__
#define __SVCHOSTBINDER_H__
struct binder_state
{
    int fd;
    void *mapped;
    size_t mapsize;
};
#ifdef __cplusplus
extern "C"
#endif
void publishSVM(struct binder_state *bs);
#endif
