#ifndef FATFS_SD_H
#define FATFS_SD_H

#include "ff.h"
#include "diskio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Low-level diskio mapping functions */
DSTATUS SD_disk_initialize(BYTE pdrv);
DSTATUS SD_disk_status(BYTE pdrv);
DRESULT SD_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
DRESULT SD_disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
DRESULT SD_disk_ioctl(BYTE pdrv, BYTE cmd, void* buff);

#ifdef __cplusplus
}
#endif

#endif /* FATFS_SD_H */
