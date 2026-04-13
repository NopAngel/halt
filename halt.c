/*
 * halt.c: A halt/poweroff/reboot wrapper for Artix Linux
 *         Based on halt.c from void-runit
 *
 * Copyright (C) 2018 Muhammad Herdiansyah
 *           (C) 2018 Artix Linux Developers
 *
 * To see the license terms of this program, see COPYING
 * (FORK)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <sys/reboot.h>

extern char *__progname;

typedef enum {NOOP, REBOOT, POWEROFF, WRITE_ONLY} action_type;
typedef enum {OPENRC, RUNIT} initsys;

/* * Optimized init detection: 
 * We use a static buffer to avoid heap allocation and leaks.
 */
initsys detect_init(void)
{
    char buf[256];
    FILE *f = fopen("/proc/1/cmdline", "r");
    if (!f) return OPENRC; // Default to OpenRC if proc is not mounted

    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    if (len > 0) {
        buf[len] = '\0';
        /* * /proc/1/cmdline can contain absolute paths like /sbin/init 
         * or just the name. We search for "runit" within the first argument.
         */
        if (strstr(buf, "runit")) return RUNIT;
    }
    return OPENRC;
}

int main(int argc, char *argv[])
{
    if (getuid() != 0)
        errx(1, "You must be root to do that!");

    int do_sync = 1;
    int do_force = 0;
    int opt;
    action_type action = NOOP;
    initsys init = detect_init();
    
    /* Initialize options buffer to avoid garbage values */
    char openrc_options[64] = "";

    /* Determine action based on program name */
    if (strcmp(__progname, "reboot") == 0)
        action = REBOOT;
    else if (strcmp(__progname, "halt") == 0 || 
             strcmp(__progname, "poweroff") == 0 || 
             strcmp(__progname, "shutdown") == 0)
        action = POWEROFF;

    while ((opt = getopt(argc, argv, "dfhinw")) != -1) {
        switch (opt) {
        case 'n':
            do_sync = 0;
            if (init == OPENRC) strncat(openrc_options, " --no-write", sizeof(openrc_options) - 1);
            break;
        case 'w':
            action = (init == RUNIT) ? NOOP : WRITE_ONLY;
            do_sync = 0;
            break;
        case 'd':
            if (init == OPENRC) strncat(openrc_options, " --no-write", sizeof(openrc_options) - 1);
            break;
        case 'f':
            do_force = 1;
            break;
        case 'h':
        case 'i':
            /* Flags accepted but ignored for compatibility */
            break;
        default:
            errx(1, "Usage: %s [-n] [-f] [-w] [-d]", __progname);
        }
    }

    if (do_sync)
        sync();

    switch (action) {
        case POWEROFF:
            if (do_force) {
                reboot(RB_POWER_OFF);
            } else {
                if (init == RUNIT)
                    execl("/usr/bin/runit-init", "init", "0", (char*)NULL);
                else
                    execl("/usr/bin/openrc-shutdown", "openrc-shutdown", "--poweroff", "now", openrc_options, (char*)NULL);
            }
            err(1, "poweroff failed");
            break;

        case REBOOT:
            if (do_force) {
                reboot(RB_AUTOBOOT);
            } else {
                if (init == RUNIT)
                    execl("/usr/bin/runit-init", "init", "6", (char*)NULL);
                else
                    execl("/usr/bin/openrc-shutdown", "openrc-shutdown", "--reboot", "now", openrc_options, (char*)NULL);
            }
            err(1, "reboot failed");
            break;

        case WRITE_ONLY:
            execl("/usr/bin/openrc-shutdown", "openrc-shutdown", "--write-only", (char*)NULL);
            err(1, "write-only failed");
            break;

        case NOOP:
            break;
    }

    return 0;
}
