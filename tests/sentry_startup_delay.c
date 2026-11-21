/* Regression fixture only: delay Sentry before main, preserving executable identity. */
#include <unistd.h>
#include <string.h>
__attribute__((constructor)) static void slow_sentry_startup(void) {
    char executable[4096];
    ssize_t count = readlink("/proc/self/exe", executable, sizeof(executable)-1);
    if (count < 0) return;
    executable[count] = 0;
    const char *name = strrchr(executable, '/');
    if (name && strcmp(name+1, "getdaq-sentry") == 0) usleep(300000);
}
