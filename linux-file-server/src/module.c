/**
 * module.c - Dynamic module loading implementation
 * Uses dlopen/dlsym for runtime plugin loading.
 */

#include "module.h"
#include <dlfcn.h>

int module_manager_init(module_manager_t *mgr) {
    if (!mgr) return -1;
    memset(mgr, 0, sizeof(module_manager_t));
    return 0;
}

int module_load(module_manager_t *mgr, const char *name, const char *path) {
    if (!mgr || !name || !path) return -1;
    if (mgr->count >= MAX_MODULES) {
        fprintf(stderr, "module_load: max modules (%d) reached\n", MAX_MODULES);
        return -1;
    }

    /* Check for duplicate name */
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->modules[i].loaded && strcmp(mgr->modules[i].name, name) == 0) {
            fprintf(stderr, "module_load: module '%s' already loaded\n", name);
            return -1;
        }
    }

    void *handle = dlopen(path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return -1;
    }

    module_t *mod = &mgr->modules[mgr->count];
    memset(mod, 0, sizeof(module_t));
    strncpy(mod->name, name, sizeof(mod->name) - 1);
    strncpy(mod->path, path, sizeof(mod->path) - 1);
    mod->handle = handle;

    /* Try to find optional functions */
    dlerror(); /* Clear any existing error */
    mod->init = (int (*)(void))dlsym(handle, "module_init");
    mod->cleanup = (void (*)(void))dlsym(handle, "module_cleanup");
    mod->on_command = (int (*)(int, int, const char *))dlsym(handle, "module_on_command");

    if (mod->init && mod->init() != 0) {
        fprintf(stderr, "module_load: init failed for '%s'\n", name);
        dlclose(handle);
        return -1;
    }

    mod->loaded = 1;
    mgr->count++;
    fprintf(stderr, "module_load: loaded '%s' from %s\n", name, path);
    return 0;
}

int module_unload(module_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    for (int i = 0; i < mgr->count; i++) {
        if (mgr->modules[i].loaded && strcmp(mgr->modules[i].name, name) == 0) {
            if (mgr->modules[i].cleanup) {
                mgr->modules[i].cleanup();
            }
            dlclose(mgr->modules[i].handle);
            mgr->modules[i].loaded = 0;

            /* Compact array: move last into this slot */
            if (i < mgr->count - 1) {
                mgr->modules[i] = mgr->modules[mgr->count - 1];
                memset(&mgr->modules[mgr->count - 1], 0, sizeof(module_t));
            }
            mgr->count--;
            fprintf(stderr, "module_unload: unloaded '%s'\n", name);
            return 0;
        }
    }
    fprintf(stderr, "module_unload: module '%s' not found\n", name);
    return -1;
}

void module_unload_all(module_manager_t *mgr) {
    if (!mgr) return;

    for (int i = mgr->count - 1; i >= 0; i--) {
        if (mgr->modules[i].loaded) {
            if (mgr->modules[i].cleanup) {
                mgr->modules[i].cleanup();
            }
            dlclose(mgr->modules[i].handle);
            mgr->modules[i].loaded = 0;
        }
    }
    mgr->count = 0;
}

int module_dispatch(module_manager_t *mgr, int fd, int cmd, const char *arg) {
    if (!mgr) return 0;

    for (int i = 0; i < mgr->count; i++) {
        if (mgr->modules[i].loaded && mgr->modules[i].on_command) {
            if (mgr->modules[i].on_command(fd, cmd, arg)) {
                return 1; /* Handled */
            }
        }
    }
    return 0; /* Not handled */
}

int module_list(const module_manager_t *mgr, char *buf, size_t buf_size) {
    if (!mgr || !buf || buf_size == 0) return 0;

    int offset = 0;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->modules[i].loaded) {
            int n = snprintf(buf + offset, buf_size - offset, "%-20s %s\n",
                            mgr->modules[i].name, mgr->modules[i].path);
            if (n > 0 && (size_t)(offset + n) < buf_size) {
                offset += n;
            } else {
                break;
            }
        }
    }
    return offset;
}
