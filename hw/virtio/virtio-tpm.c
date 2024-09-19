/*
 * A virtio device implementing a hardware tpm in communication with virtio-queue.
 *
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/iov.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/virtio/virtio.h"
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio-tpm.h"
#include "hw/tpm/tpm_prop.h"
#include "sysemu/tpm.h"
#include "sysemu/runstate.h"
#include "qom/object_interfaces.h"
#include "trace.h"

static void tpm_virtio_handle(VirtIODevice *vdev, VirtQueue *vq) // TODO: 应该指明处理方向
{
    // VirtIOTPM* vtpm = VIRTIO_TPM(vdev);
    // CRBState* s = CRB(dev); // 是否需要 CRB 参与？

    ssize_t len;
    VirtQueueElement *elem;
    // TPMBackendCmd cmd;
    uint8_t *buf = NULL;

    elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
    len = iov_from_buf(elem->in_sg, elem->in_num, 0,
                        buf, 6);
    len = len - 1;

}

static uint64_t get_features(VirtIODevice *vdev, uint64_t f, Error **errp)
{
    return f;
}

static void virtio_tpm_set_status(VirtIODevice *vdev, uint8_t status)
{
    // VirtIOTPM *vtpm = VIRTIO_TPM(vdev);

    if (!vdev->vm_running) {
        return;
    }
    vdev->status = status;

}

static void virtio_tpm_device_realize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    VirtIOTPM *vtpm = VIRTIO_TPM(dev);

    virtio_init(vdev, VIRTIO_ID_TPM, 0);

    vtpm->vq = virtio_add_queue(vdev, 8, tpm_virtio_handle); // 设备侧：将 CMD 提取出来使用 tpm_passthrough_unix_tx_bufs
}

static void virtio_tpm_device_unrealize(DeviceState *dev)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    // VirtIOTPM *vtpm = VIRTIO_TPM(dev);

    virtio_del_queue(vdev, 0);
    virtio_cleanup(vdev);
}

static const VMStateDescription vmstate_virtio_tpm = {
    .name = "virtio-tpm",
    .minimum_version_id = 1,
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_VIRTIO_DEVICE,
        VMSTATE_END_OF_LIST()
    },
};

static Property virtio_tpm_properties[] = {
    /* Set a default rate limit of 2^47 bytes per minute or roughly 2TB/s.  If
     * you have an entropy source capable of generating more entropy than this
     * and you can pass it through via virtio-rng, then hats off to you.  Until
     * then, this is unlimited for all practical purposes.
     */
    DEFINE_PROP_LINK("tpm", VirtIOTPM, conf.tpm, TYPE_RNG_BACKEND, RngBackend *),

    DEFINE_PROP_END_OF_LIST(),
};

static void virtio_tpm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);

    device_class_set_props(dc, virtio_tpm_properties);
    dc->vmsd = &vmstate_virtio_tpm;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    vdc->realize = virtio_tpm_device_realize;
    vdc->unrealize = virtio_tpm_device_unrealize;
    vdc->get_features = get_features;
    vdc->set_status = virtio_tpm_set_status;
}

static const TypeInfo virtio_tpm_info = {
    .name = TYPE_VIRTIO_TPM,
    .parent = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtIOTPM),
    .class_init = virtio_tpm_class_init,
};

static void virtio_register_types(void)
{
    type_register_static(&virtio_tpm_info);
}

type_init(virtio_register_types)
