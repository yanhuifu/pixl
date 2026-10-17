#ifndef AMIIDB_API_V3_H
#define AMIIDB_API_V3_H
#include "amiibo_helper.h"
#include "amiidb_v3_machines.h"
#include "vfs.h"
#include <stdio.h>
/* A separate file per rider preserves console writes across menu transitions. */
static inline void amiidb_v3_path(char path[64], const ntag_t *tag) {
    snprintf(path, 64, "/amiibo/v3/%08lx%08lx.bin",
        (unsigned long)to_little_endian_int32(tag->data + 84),
        (unsigned long)to_little_endian_int32(tag->data + 88));
}
static inline bool amiidb_v3_save(ntag_t *tag) {
    vfs_driver_t *driver = vfs_get_driver(VFS_DRIVE_EXT);
    if (!driver || !driver->mounted()) return false;
    vfs_obj_t obj;
    if (driver->stat_file("/amiibo/v3", &obj) != VFS_OK) driver->create_dir("/amiibo/v3");
    char path[64];
    amiidb_v3_path(path, tag);
    return driver->write_file_data(path, tag->data, NTAG_I2C_2K_DATA_SIZE) == NTAG_I2C_2K_DATA_SIZE;
}
static inline bool amiidb_v3_load(uint32_t head, uint32_t tail, ntag_t *tag) {
    vfs_driver_t *driver = vfs_get_driver(VFS_DRIVE_EXT);
    if (!driver || !driver->mounted()) return false;
    char path[64];
    snprintf(path, sizeof(path), "/amiibo/v3/%08lx%08lx.bin", (unsigned long)head, (unsigned long)tail);
    vfs_obj_t obj;
    if (driver->stat_file(path, &obj) != VFS_OK || obj.size != NTAG_I2C_2K_DATA_SIZE) return false;
    memset(tag, 0, sizeof(*tag));
    if (driver->read_file_data(path, tag->data, sizeof(tag->data)) != NTAG_I2C_2K_DATA_SIZE ||
        to_little_endian_int32(tag->data + 84) != head || to_little_endian_int32(tag->data + 88) != tail) return false;
    tag->type = NTAG_I2C_PLUS_2K;
    return true;
}
static inline bool amiidb_v3_remove(const ntag_t *tag) {
    vfs_driver_t *driver = vfs_get_driver(VFS_DRIVE_EXT);
    if (!driver || !driver->mounted()) return false;
    char path[64];
    amiidb_v3_path(path, tag);
    return driver->remove_file(path) == VFS_OK;
}
#endif
