#include "amiibo_helper.h"
#include "amiidb_scene.h"
#include "app_amiidb.h"
#include "mui_list_view.h"
#include "nrf_log.h"

#include "db_header.h"
#include "mui_icons.h"
#include "settings.h"
#include <math.h>
#include "vfs.h"
#include "vfs_meta.h"
#include <stdio.h>
#include <string.h>

#define PLATFORM_FIRST 200
#define PLATFORM_LAST 203
#define PLATFORM_MENU 204 /* virtual view, not a database game ID */
#define VISIBILITY_FILE "/db-visible.bin"
/* Stable game IDs, independent of language and sort order. */
static uint8_t visibility[37]; /* magic[4], selected[32], show_all */
static bool is_platform(uint16_t id) { return id >= PLATFORM_FIRST && id <= PLATFORM_LAST; }
static bool game_selected(uint8_t id) { return (visibility[4 + id / 8] & (1u << (id % 8))) != 0; }
static void visibility_load(void) {
    memset(visibility, 0, sizeof(visibility));
    memcpy(visibility, "DBG1", 4);
    uint8_t saved[sizeof(visibility)];
    vfs_driver_t *driver = vfs_get_default_driver();
    if (driver && driver->mounted() &&
        driver->read_file_data(VISIBILITY_FILE, saved, sizeof(saved)) == (int32_t)sizeof(saved) &&
        memcmp(saved, "DBG1", 4) == 0 && saved[36] <= 1) {
        memcpy(visibility, saved, sizeof(saved));
    }
}
static bool visibility_save(void) {
    vfs_driver_t *driver = vfs_get_default_driver();
    if (!driver || !driver->mounted()) return false;
    if (driver->write_file_data(VISIBILITY_FILE, visibility, sizeof(visibility)) != (int32_t)sizeof(visibility)) return false;
    vfs_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.has_flags = true;
    meta.flags = VFS_OBJ_FLAG_HIDDEN;
    uint8_t encoded[VFS_MAX_META_LEN];
    vfs_meta_encode(encoded, sizeof(encoded), &meta);
    driver->update_file_meta(VISIBILITY_FILE, encoded, sizeof(encoded));
    return true;
}
static const char *game_name(const db_game_t *game) {
    return settings_get_data()->language == LANGUAGE_ZH_HANS ? game->name_cn : game->name_en;
}
static void selected_count(uint8_t platform, unsigned *selected, unsigned *total) {
    *selected = *total = 0;
    for (const db_game_t *game = game_list; game->game_id; ++game) {
        if (game->parent_game_id == platform) {
            ++*total;
            if (game_selected(game->game_id)) ++*selected;
        }
    }
}

static void amiidb_scene_game_list_reload(app_amiidb_t *app);

static void amiidb_scene_game_list_restore_position(app_amiidb_t *app) {
    uint8_t depth = app->game_id_index;
    if (app->game_focus_path[depth] < mui_list_view_item_size(app->p_list_view)) {
        mui_list_view_set_focus(app->p_list_view, app->game_focus_path[depth]);
        mui_list_view_set_scroll_offset(app->p_list_view, app->game_scroll_path[depth]);
    }
}

static bool amiidb_scene_game_list_back(void *user_data) {
    app_amiidb_t *app = user_data;
    if (app->game_id_index == 0) {
        return false;
    }
    app->game_id_index--;
    amiidb_scene_game_list_reload(app);
    amiidb_scene_game_list_restore_position(app);
    return true;
}

static void amiidb_scene_game_list_list_view_on_selected(mui_list_view_event_t event, mui_list_view_t *p_list_view,
                                                         mui_list_item_t *p_item) {
    uint16_t icon = p_item->icon;
    app_amiidb_t *app = mui_list_view_get_user_data(p_list_view);
    uint16_t current = app->game_id_path[app->game_id_index];
    if (icon == ICON_SETTINGS && current == 0) {
        if (event != MUI_LIST_VIEW_EVENT_SELECTED) return;
        if ((size_t)app->game_id_index + 1 >= sizeof(app->game_id_path) / sizeof(app->game_id_path[0])) return;
        app->game_focus_path[app->game_id_index] = mui_list_view_get_focus(p_list_view);
        app->game_scroll_path[app->game_id_index] = mui_list_view_get_scroll_offset(p_list_view);
        app->game_id_path[++app->game_id_index] = PLATFORM_MENU;
        app->game_focus_path[app->game_id_index] = 0;
        app->game_scroll_path[app->game_id_index] = 0;
        amiidb_scene_game_list_reload(app);
        return;
    }
    if ((icon == ICON_SETTINGS && current == PLATFORM_MENU) ||
        (is_platform(current) && (icon == ICON_CHECKBOX_OFF || icon == ICON_CHECKBOX_ON))) {
        if (event != MUI_LIST_VIEW_EVENT_SELECTED) return;
        uint8_t previous[sizeof(visibility)];
        memcpy(previous, visibility, sizeof(previous));
        if (icon == ICON_SETTINGS) visibility[36] ^= 1;
        else {
            const db_game_t *game = p_item->user_data;
            visibility[4 + game->game_id / 8] ^= (1u << (game->game_id % 8));
        }
        if (!visibility_save()) {
            memcpy(visibility, previous, sizeof(previous));
            mui_toast_view_show(app->p_toast_view,
                settings_get_data()->language == LANGUAGE_ZH_HANS ? "保存失败" : "Save failed");
            return;
        }
        uint16_t focus = mui_list_view_get_focus(p_list_view);
        uint16_t scroll = mui_list_view_get_scroll_offset(p_list_view);
        amiidb_scene_game_list_reload(app);
        mui_list_view_set_focus(p_list_view, focus);
        mui_list_view_set_scroll_offset(p_list_view, scroll);
        return;
    }
    switch (icon) {
    case ICON_EXIT:
        if (app->game_id_index <= 0) {
            mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, AMIIDB_SCENE_MAIN);
        } else {
            amiidb_scene_game_list_back(app);
        }
        break;

    case ICON_FOLDER: {
        const db_game_t *p_game = p_item->user_data;
        if ((size_t)app->game_id_index + 1 >= sizeof(app->game_id_path) / sizeof(app->game_id_path[0])) {
            break;
        }
        app->game_focus_path[app->game_id_index] = mui_list_view_get_focus(p_list_view);
        app->game_scroll_path[app->game_id_index] = mui_list_view_get_scroll_offset(p_list_view);
        app->game_id_path[++app->game_id_index] = p_game->game_id;
        app->game_focus_path[app->game_id_index] = 0;
        app->game_scroll_path[app->game_id_index] = 0;
        amiidb_scene_game_list_reload(app);
    } break;

    case ICON_FILE: {
        const db_amiibo_t *p_amiibo = p_item->user_data;

        if(is_valid_amiibo_v3(p_amiibo->head, p_amiibo->tail)){
            mui_toast_view_show(app->p_toast_view, _T(APP_AMIIDB_NOT_SUPPORT_V3));
        }else{
            app->cur_amiibo = p_amiibo;
            app->cur_slot_index = mui_list_view_get_focus(p_list_view);
            app->prev_scene_id = AMIIDB_SCENE_GAME_LIST;
            mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, AMIIDB_SCENE_AMIIBO_DETAIL);
        }
    } break;
    }
}

/* Sorting is called only on directory/checkbox rows, before tag rows or controls. */
static int amiidb_scene_game_list_list_view_sort_cb(const mui_list_item_t *a, const mui_list_item_t *b) {
    const db_game_t *ga = a->user_data;
    const db_game_t *gb = b->user_data;
    if (settings_get_data()->amiidb_sort_column == AMIIDB_SORT_COLUMN_NAME) {
        return strcmp(game_name(ga), game_name(gb));
    }
    return gb->order - ga->order;
}

static void amiidb_scene_game_list_reload(app_amiidb_t *app) {
    settings_data_t *p_settings_data = settings_get_data();
    char txt[192];

    // clear list view
    mui_list_view_clear_items(app->p_list_view);

    uint16_t cur_game_id = app->game_id_path[app->game_id_index];
    if (cur_game_id == 0) {
        /* "View" opens the selected games directly, as in the reference video. */
        for (const db_game_t *game = game_list; game->game_id; ++game) {
            if (is_platform(game->parent_game_id) && (visibility[36] || game_selected(game->game_id))) {
                mui_list_view_add_item(app->p_list_view, ICON_FOLDER, game_name(game), (void *)game);
            }
        }
        mui_list_view_sort(app->p_list_view, amiidb_scene_game_list_list_view_sort_cb);
        mui_list_view_add_item(app->p_list_view, ICON_SETTINGS,
            p_settings_data->language == LANGUAGE_ZH_HANS ? "显示更多游戏" : "More games", NULL);
    } else if (cur_game_id == PLATFORM_MENU) {
        for (uint16_t id = PLATFORM_FIRST; id <= PLATFORM_LAST; ++id) {
            for (const db_game_t *game = game_list; game->game_id; ++game) {
                if (game->game_id != id) continue;
                unsigned selected, total;
                selected_count(id, &selected, &total);
                snprintf(txt, sizeof(txt), "%s (%u/%u)", game_name(game), selected, total);
                mui_list_view_add_item(app->p_list_view, ICON_FOLDER, txt, (void *)game);
            }
        }
        mui_list_view_add_item(app->p_list_view, ICON_SETTINGS,
            p_settings_data->language == LANGUAGE_ZH_HANS ?
                (visibility[36] ? "显示全部 [开]" : "显示全部 [关]") :
                (visibility[36] ? "Show all [ON]" : "Show all [OFF]"), NULL);
    } else {
        for (const db_game_t *game = game_list; game->game_id; ++game) {
            if (game->parent_game_id != cur_game_id) continue;
            if (is_platform(cur_game_id)) {
                mui_list_view_add_item(app->p_list_view,
                    game_selected(game->game_id) ? ICON_CHECKBOX_ON : ICON_CHECKBOX_OFF,
                    game_name(game), (void *)game);
            } else {
                snprintf(txt, sizeof(txt), "%s (%u)", game_name(game), game->link_cnt);
                mui_list_view_add_item(app->p_list_view, ICON_FOLDER, txt, (void *)game);
            }
        }
        mui_list_view_sort(app->p_list_view, amiidb_scene_game_list_list_view_sort_cb);
    }

    // add link list by page
    const db_link_t *p_link = link_list;
    uint16_t link_cnt = 0;
    uint16_t add_cnt = 0;
    while (p_link->game_id > 0) {
        if (p_link->game_id == cur_game_id) {
            link_cnt++;
            if (add_cnt < LIST_VIEW_ITEM_MAX_COUNT) {
                const db_amiibo_t *p_amiibo = get_amiibo_by_id(p_link->head, p_link->tail);
                if (p_amiibo) {
                    const char *name = get_amiibo_display_name((db_amiibo_t *)p_amiibo);
                    mui_list_view_add_item(app->p_list_view, ICON_FILE, name, (void *)p_amiibo);
                }
                add_cnt++;
            }
        }
        p_link++;
    }

    if (link_cnt > LIST_VIEW_ITEM_MAX_COUNT) {
        mui_list_view_add_item(app->p_list_view, ICON_ERROR, getLangString(_L_APP_AMIIDB_MORE), (void *)0);
    }

    mui_list_view_add_item(app->p_list_view, ICON_EXIT, getLangString(_L_APP_AMIIDB_BACK), (void *)0);
    mui_list_view_set_selected_cb(app->p_list_view, amiidb_scene_game_list_list_view_on_selected);
    mui_list_view_set_user_data(app->p_list_view, app);
    mui_list_view_set_focus(app->p_list_view, 0);
    mui_list_view_set_scroll_offset(app->p_list_view, 0);
    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, AMIIDB_VIEW_ID_LIST);
}

void amiidb_scene_game_list_on_enter(void *user_data) {
    app_amiidb_t *app = (app_amiidb_t *)user_data;
    visibility_load();
    if (app->game_id_index >= sizeof(app->game_id_path) / sizeof(app->game_id_path[0])) {
        app->game_id_index = 0;
        app->game_id_path[0] = 0;
    }
    mui_scene_dispatcher_set_back_cb(app->p_scene_dispatcher, amiidb_scene_game_list_back);
    amiidb_scene_game_list_reload(app);
    amiidb_scene_game_list_restore_position(app);
}

void amiidb_scene_game_list_on_exit(void *user_data) {
    app_amiidb_t *app = (app_amiidb_t *)user_data;
    app->game_focus_path[app->game_id_index] = mui_list_view_get_focus(app->p_list_view);
    app->game_scroll_path[app->game_id_index] = mui_list_view_get_scroll_offset(app->p_list_view);
    mui_scene_dispatcher_set_back_cb(app->p_scene_dispatcher, NULL);
    mui_list_view_clear_items(app->p_list_view);
}
