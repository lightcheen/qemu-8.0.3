/*
 * Virtio TPM Support

 */

#ifndef QEMU_VIRTIO_TPM_H
#define QEMU_VIRTIO_TPM_H

#include "hw/virtio/virtio.h"
#include "sysemu/tpm.h"
#include "sysemu/tpm_backend.h"
#include "standard-headers/linux/virtio_tpm.h"
#include "qom/object.h"

#define TYPE_VIRTIO_TPM "virtio-tpm-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOTPM, VIRTIO_TPM)
#define VIRTIO_TPM_GET_PARENT_CLASS(obj) \
        OBJECT_GET_PARENT_CLASS(obj, TYPE_VIRTIO_TPM)

// virtio_tpm_properties 没有额外的属性，因此不需要。不过之后可以添加属性用于指明其依靠 TIS/CRB
struct VirtIOTPMConf { };

struct VirtIOTPM {
    VirtIODevice parent_obj;

    /* Only one vq - guest puts buffer(s) on it when it needs entropy */
    VirtQueue *vq;

    // VirtIOTPMConf conf;
};

#endif
