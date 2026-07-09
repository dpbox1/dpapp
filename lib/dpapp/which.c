#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char* normalize_path(const char* path)
{
    static char _result[PATH_MAX];
    char temp[PATH_MAX];
    strncpy(temp, path, sizeof(temp));
    temp[PATH_MAX - 1] = '\0';

    char* components[PATH_MAX];
    int top = 0;

    char* token = strtok(temp, "/");
    while (token) {
        if (strcmp(token, "..") == 0) {
            if (top > 0)
                top--;
        } else if (strcmp(token, ".") == 0) {
            // 忽略
        } else if (*token) {
            components[top++] = token;
        }
        token = strtok(NULL, "/");
    }

    _result[0] = '\0';
    strcat(_result, "/");

    for (int i = 0; i < top; i++) {
        strcat(_result, components[i]);
        if (i < top - 1)
            strcat(_result, "/");
    }

    return _result;
}

const char* absolute_path(const char* fullpath)
{
    static char _abs[PATH_MAX + 1];
    if (fullpath[0] == '/') {
        snprintf(_abs, sizeof(_abs), "%s", fullpath);
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            perror("getcwd");
            return NULL;
        }
        snprintf(_abs, sizeof(_abs), "%s/%s", cwd, fullpath);
    }
    return normalize_path(_abs);
}

const char* find_executable(const char* cmd)
{
    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) {
            return absolute_path(cmd);
        } else {
            return NULL;
        }
    }

    char* path_env = getenv("PATH");
    if (!path_env) {
        return NULL;
    }

    char* path_copy = strdup(path_env);
    if (!path_copy) {
        return NULL;
    }

    char* dir = strtok(path_copy, ":");
    while (dir) {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, cmd);
        if (access(full, X_OK) == 0) {
            free(path_copy);
            return absolute_path(full);
        }
        dir = strtok(NULL, ":");
    }
    free(path_copy);
    return NULL;
}

// int main(int argc, char *argv[]) {
//     if (argc < 2) {
//         fprintf(stderr, "Usage: %s command...\n", argv[0]);
//         return 1;
//     }

//     for (int i = 1; i < argc; i++) {
//         const char* p = find_executable(argv[i]);
//         if(p) {
//             printf("%s\n", p);
//         } else {
//             fprintf(stderr, "%s: command not found\n", argv[i]);
//         }
//     }

//     return 0;
// }

bool is_dir(const char* path)
{
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return false;
    }
    return S_ISDIR(path_stat.st_mode);
}

bool is_file(const char* path)
{
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return false;
    }
    return S_ISREG(path_stat.st_mode) || S_ISLNK(path_stat.st_mode);
}

bool mkdir_p(const char* path)
{
    char tmp[4096] = "";
    const char* normalized = normalize_path(path);
    if (normalized[0] == '\0') {
        return false;
    }
    strncpy(tmp, normalized, sizeof(tmp) - 1);

    for (char* p = tmp + 1; *p; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return false;
        }
        *p = '/';
    }

    return (mkdir(tmp, 0755) == 0 || errno == EEXIST);
}
