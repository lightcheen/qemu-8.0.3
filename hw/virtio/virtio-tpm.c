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
#include "sysemu/tpm_util.h"
#include "sysemu/runstate.h"
#include "qom/object_interfaces.h"
#include "trace.h"
#include "chardev/char-fe.h"
#include "io/channel-socket.h"
#include "backends/tpm/tpm_ioctl.h"
#include "backends/tpm/tpm_int.h"

#define TYPE_TPM_EMULATOR "tpm-emulator"
OBJECT_DECLARE_SIMPLE_TYPE(TPMEmulator, TPM_EMULATOR)

typedef struct TPMBlobBuffers {
    uint32_t permanent_flags;
    TPMSizedBuffer permanent;

    uint32_t volatil_flags;
    TPMSizedBuffer volatil;

    uint32_t savestate_flags;
    TPMSizedBuffer savestate;
} TPMBlobBuffers;


struct TPMEmulator {
    TPMBackend parent;

    TPMEmulatorOptions *options;
    CharBackend ctrl_chr;
    QIOChannel *data_ioc;
    TPMVersion tpm_version;
    ptm_cap caps; /* capabilities of the TPM */
    uint8_t cur_locty_number; /* last set locality */
    Error *migration_blocker;

    QemuMutex mutex;

    unsigned int established_flag:1;
    unsigned int established_flag_cached:1;

    TPMBlobBuffers state_blobs;

    bool relock_storage;
    VMChangeStateEntry *vmstate;
};


static void tpm_virtio_handle(VirtIODevice* vdev, VirtQueue* vq)
{
    ssize_t ret;
    uint8_t *in;
    uint32_t in_len;
    uint8_t *out;
    uint32_t out_len;
    VirtQueueElement *elem;
    Error **errp = NULL;
    int i=0;

    VirtIOTPM* vtpm = VIRTIO_TPM(vdev);
    TPMBackend *tpmbe = vtpm->tpmbe;
    TPMEmulator* tpm_emu = TPM_EMULATOR(tpmbe);
    
    elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
    elem->out_sg->iov_base = (uint8_t * ) malloc (2048 * sizeof(uint8_t));
    if(!elem){
        printf("elem is NULL\n");
    }
    printf("*************begin\n");
    printf("elem->index : %d\n",elem->index);
    printf("elem->len : %d\n",elem->len);
    printf("elem->ndescs : %d\n",elem->ndescs);
    printf("elem->out_num : %d\n",elem->out_num);
    printf("elem->in_num : %d\n",elem->in_num);
    printf("elem->in_addr : %ld\n",elem->in_addr[0]);
    printf("elem->out_addr : %ld\n",elem->out_addr[0]);

    in = (uint8_t*) elem->in_sg[0].iov_base;
    printf("elem->in_sg->iov_len : %ld\n",elem->in_sg[0].iov_len);
    printf("elem->in_sg->iov_base : \n");
    for(i=0 ;(i< (elem->in_sg[0].iov_len))&&(i<128);i++){
        if(i%16 == 0){
            printf("\n");
        }
        printf("%02x ",in[i]);
    }
    printf("\n");

    out = (uint8_t*)elem->out_sg->iov_base;
    printf("elem->out_sg->iov_len : %ld\n",elem->out_sg[0].iov_len);
    printf("elem->out_sg->iov_base : \n");
    for(i=0 ;(i< (elem->out_sg[0].iov_len))&&(i<128);i++){
        if(i%16 == 0){
            printf("\n");
        }
        printf("%02x ",out[i]);
    }
    printf("\n******************begin\n");

    in_len = elem->in_sg->iov_len;
    if(!elem->in_sg->iov_base){
        printf("elem->out_sg[0].iov_base is NULL\n");
    }
    if(!in){
        printf("in is null\n");
    }
    printf("in_len is %d\n",in_len);

    if(!tpm_emu->data_ioc){
        printf("tpm_emu->data_ioc is null\n");
    }
    if(!elem->out_sg->iov_base){
        printf("elem->out_sg->iov_baseis null\n");
    }

    ret = qio_channel_write_all(tpm_emu->data_ioc, (char *)in, in_len, errp);
    if (ret != 0) {
        printf("qio_channel_write_all failed\n");
    }

    //out = (uint8_t*)elem->out_sg->iov_base;
    ret = qio_channel_read_all(tpm_emu->data_ioc, (char *)out,
              sizeof(struct tpm_resp_hdr), errp);
    if (ret != 0) {
        printf("qio_channel_read_all failed\n");
    }
    printf("[tpm_virtio_handle] %02x %02x (%02x %02x %02x %02x) \n", out[0], out[1], out[8], out[9], out[10], out[11]);

    ret = qio_channel_read_all(tpm_emu->data_ioc,
              (char *)out + sizeof(struct tpm_resp_hdr),
              tpm_cmd_get_size(out) - sizeof(struct tpm_resp_hdr), errp);
    if (ret != 0) {
        printf("qio_channel_read_all failed\n");
    }

    out_len = tpm_cmd_get_size(out) + sizeof(struct tpm_resp_hdr) - 10;
    //elem->out_sg[0].iov_len = out_len;
    //elem->out_num = 1;
    printf("out_len : %d\n",out_len);

    printf("*************end\n");
    printf("elem->index : %d\n",elem->index);
    printf("elem->len : %d\n",elem->len);
    printf("elem->ndescs : %d\n",elem->ndescs);
    printf("elem->out_num : %d\n",elem->out_num);
    printf("elem->in_num : %d\n",elem->in_num);
    printf("elem->in_addr : %ld\n",elem->in_addr[0]);
    printf("elem->out_addr : %ld\n",elem->out_addr[0]);
    printf("elem->in_sg->iov_len : %ld\n",elem->in_sg[0].iov_len);
    printf("elem->in_sg->iov_base : \n");
    for(i=0 ;(i< (elem->in_sg[0].iov_len))&&(i<128);i++){
        if(i%16 == 0){
            printf("\n");
        }
        printf("%02x ",in[i]);
    }
    printf("\n");
    printf("elem->out_sg->iov_len : %ld\n",elem->out_sg[0].iov_len);
    printf("elem->out_sg->iov_base : \n");
    for(i=0 ;(i< out_len)&&(i<128);i++){
        if(i%16 == 0){
            printf("\n");
        }
        printf("%02x ",out[i]);
    }
    printf("\n******************end\n");
    virtqueue_push(vq, elem, out_len);
    virtio_notify(vdev, vq);

    g_free(elem);

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
    vtpm->data_vq = virtio_add_queue(vdev, 8, tpm_virtio_handle);  // 设备侧：将 CMD 提取出来使用 tpm_passthrough_unix_tx_bufs

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
    DEFINE_PROP_TPMBE("tpmdev", VirtIOTPM, tpmbe),
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
