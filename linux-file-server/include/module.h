/**
 * module.h - Dynamic module loading interface
 * Uses dlopen/dlsym to extend server with runtime plugins.
 */

#ifndef MODULE_H
#define MODULE_H

#include "common.h"

#define MAX_MODULES 16

typedef struct {
    char name[64];
    char path[256];
    void *handle;          /* dlopen handle */
    int (*init)(void);     /* Module init function */
    void (*cleanup)(void); /* Module cleanup function */
    int (*on_command)(int fd, int cmd, const char *arg); /* Command handler */
    int loaded;
} module_t;

typedef struct {
    module_t modules[MAX_MODULES];
    int count;
} module_manager_t;

/* Initialize module manager */
int module_manager_init(module_manager_t *mgr);

/* Load a module from .so file */
int module_load(module_manager_t *mgr, const char *name, const char *path);

/* Unload a module */
int module_unload(module_manager_t *mgr, const char *name);

/* Unload all modules */
void module_unload_all(module_manager_t *mgr);

/* Dispatch command to modules. Returns 1 if handled, 0 if not. */
int module_dispatch(module_manager_t *mgr, int fd, int cmd, const char *arg);

/* List loaded modules */
int module_list(const module_manager_t *mgr, char *buf, size_t buf_size);

#endif /* MODULE_H */
