#ifndef MUI_SCENE_DISPATCHER_H
#define MUI_SCENE_DISPATCHER_H

#include "mui_common.h"
typedef void (*mui_scene_enter_cb_t)(void *user_data);
typedef void (*mui_scene_exit_cb_t)(void *user_data);
/* Back handler: return true if the scene handled the back action
 * internally (e.g. navigating up a directory tree inside the scene).
 * Return false to let the dispatcher pop the scene stack. */
typedef bool (*mui_scene_back_handler_t)(void *user_data);

ARRAY_DEF(scene_id_stack, uint32_t, M_DEFAULT_OPLIST);

typedef struct {
    uint32_t scene_id;
    mui_scene_enter_cb_t enter_cb;
    mui_scene_exit_cb_t exit_cb;
} mui_scene_t;

typedef struct {
    void *user_data;
    const mui_scene_t *p_scene_defines;
    uint32_t scene_num;
    uint32_t default_scene_id;
    scene_id_stack_t scene_id_stack;
    mui_scene_back_handler_t back_handler;
} mui_scene_dispatcher_t;

mui_scene_dispatcher_t* mui_scene_dispatcher_create();
void mui_scene_dispatcher_free(mui_scene_dispatcher_t* p_dispatcher);
void mui_scene_dispatcher_exit(mui_scene_dispatcher_t* p_dispatcher);

void mui_scene_dispatcher_set_scene_defines(mui_scene_dispatcher_t *p_dispatcher, const mui_scene_t *p_scene_defines,
                                            uint32_t scene_num);
void mui_scene_dispatcher_set_user_data(mui_scene_dispatcher_t *p_dispatcher, void *user_data);
void mui_scene_dispatcher_next_scene(mui_scene_dispatcher_t *p_dispatcher, uint32_t scene_id);
void mui_scene_dispatcher_previous_scene(mui_scene_dispatcher_t *p_dispatcher);
void mui_scene_dispatcher_back_scene(mui_scene_dispatcher_t *p_dispatcher, uint32_t step);

/* Register a back handler for the current scene.  Called by scenes that
 * manage their own internal navigation levels (e.g. a folder tree). */
void mui_scene_dispatcher_set_back_handler(mui_scene_dispatcher_t *p_dispatcher, mui_scene_back_handler_t handler);
/* Handle a back request: if a back handler is registered and returns
 * true, the scene handled it internally.  Otherwise pop the scene
 * stack (previous_scene).  Returns true if handled internally. */
bool mui_scene_dispatcher_handle_back(mui_scene_dispatcher_t *p_dispatcher);

uint32_t mui_scene_dispatcher_current_scene(mui_scene_dispatcher_t *p_dispatcher);
uint32_t mui_scene_dispatcher_scene_stack_size(mui_scene_dispatcher_t *p_dispatcher);

void inline mui_mui_scene_dispatcher_set_default_scene_id(mui_scene_dispatcher_t *p_dispatcher, uint32_t default_scene_id) {
    p_dispatcher->default_scene_id = default_scene_id;
}

#endif
