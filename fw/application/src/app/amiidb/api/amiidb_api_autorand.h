#ifndef AMIIDB_API_AUTORAND_H
#define AMIIDB_API_AUTORAND_H

#include "db_header.h"
#include "vfs.h"
#include "vfs_meta.h"
#include <string.h>

/* Independent per-game choices; legacy global auto_gen_amiibo is not imported. */
#define AMIIDB_AUTORAND_FILE "/db-autorand.bin"
#define AMIIDB_AUTORAND_SEARCH_ZELDA 254
#define AMIIDB_AUTORAND_OTHER 255

static inline bool amiidb_autorand_default(uint8_t game_id) {
    if (game_id == AMIIDB_AUTORAND_SEARCH_ZELDA) return true;
    if (game_id == AMIIDB_AUTORAND_OTHER) return false;
    /* Include children of Zelda folders, independent of translated names. */
    for (unsigned depth = 0; game_id && depth < 8; ++depth) {
        const db_game_t *game = game_list;
        while (game->game_id && game->game_id != game_id) ++game;
        if (!game->game_id) return false;
        if (strstr(game->name_en, "Zelda") != NULL || strstr(game->name_en, "Hyrule Warriors") != NULL) return true;
        game_id = game->parent_game_id;
    }
    return false;
}

static inline void amiidb_autorand_load(uint8_t data[68]) {
    memset(data, 0, 68);
    memcpy(data, "DRA1", 4);
    vfs_driver_t *driver = vfs_get_default_driver();
    uint8_t saved[68];
    if (driver && driver->mounted() && driver->read_file_data(AMIIDB_AUTORAND_FILE, saved, 68) == 68 &&
        memcmp(saved, "DRA1", 4) == 0) memcpy(data, saved, 68);
}

static inline bool amiidb_autorand_get(uint8_t game_id) {
    uint8_t data[68];
    amiidb_autorand_load(data);
    uint8_t bit = (uint8_t)(1u << (game_id % 8));
    if (data[4 + game_id / 8] & bit) return (data[36 + game_id / 8] & bit) != 0;
    return amiidb_autorand_default(game_id);
}

static inline bool amiidb_autorand_set(uint8_t game_id, bool enabled) {
    uint8_t data[68];
    amiidb_autorand_load(data);
    uint8_t bit = (uint8_t)(1u << (game_id % 8));
    data[4 + game_id / 8] |= bit;
    if (enabled) data[36 + game_id / 8] |= bit;
    else data[36 + game_id / 8] &= (uint8_t)~bit;
    vfs_driver_t *driver = vfs_get_default_driver();
    if (!driver || !driver->mounted() || driver->write_file_data(AMIIDB_AUTORAND_FILE, data, 68) != 68) return false;
    vfs_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.has_flags = true;
    meta.flags = VFS_OBJ_FLAG_HIDDEN;
    uint8_t encoded[VFS_MAX_META_LEN];
    vfs_meta_encode(encoded, sizeof(encoded), &meta);
    driver->update_file_meta(AMIIDB_AUTORAND_FILE, encoded, sizeof(encoded));
    return true;
}
#endif
