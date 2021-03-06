/*
 * Copyright (C) 2014-2016 Firejail Authors
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "firejail.h"
#include <sys/stat.h>

#define MAX_READ 8192				  // line buffer for profile files

static int initialized = 0;
static int cfg_val[CFG_MAX];
char *xephyr_screen = "800x600";
char *xephyr_extra_params = "";

int checkcfg(int val) {
	EUID_ASSERT();
	assert(val < CFG_MAX);
	int line = 0;

	if (!initialized) {
		// initialize defaults
		int i;
		for (i = 0; i < CFG_MAX; i++)
			cfg_val[i] = 1; // most of them are enabled by default

		cfg_val[CFG_RESTRICTED_NETWORK] = 0; // disabled by default
		cfg_val[CFG_FORCE_NONEWPRIVS] = 0; // disabled by default
		
		// open configuration file
		char *fname;
		if (asprintf(&fname, "%s/firejail.config", SYSCONFDIR) == -1)
			errExit("asprintf");

		FILE *fp = fopen(fname, "r");
		if (!fp) {
#ifdef HAVE_GLOBALCFG			
			fprintf(stderr, "Warning: Firejail configuration file %s not found\n", fname);
			exit(1);
#else
			initialized = 1;
			return	cfg_val[val];
#endif	
		}
		
		// read configuration file
		char buf[MAX_READ];
		while (fgets(buf,MAX_READ, fp)) {
			line++;
			if (*buf == '#' || *buf == '\n') 
				continue;

			// parse line				
			char *ptr = line_remove_spaces(buf);
			if (!ptr)
				continue;
			
			// file transfer	
			if (strncmp(ptr, "file-transfer ", 14) == 0) {
				if (strcmp(ptr + 14, "yes") == 0)
					cfg_val[CFG_FILE_TRANSFER] = 1;
				else if (strcmp(ptr + 14, "no") == 0)
					cfg_val[CFG_FILE_TRANSFER] = 0;
				else
					goto errout;
			}
			// x11
			else if (strncmp(ptr, "x11 ", 4) == 0) {
				if (strcmp(ptr + 4, "yes") == 0)
					cfg_val[CFG_X11] = 1;
				else if (strcmp(ptr + 4, "no") == 0)
					cfg_val[CFG_X11] = 0;
				else
					goto errout;
			}
			// bind
			else if (strncmp(ptr, "bind ", 5) == 0) {
				if (strcmp(ptr + 5, "yes") == 0)
					cfg_val[CFG_BIND] = 1;
				else if (strcmp(ptr + 5, "no") == 0)
					cfg_val[CFG_BIND] = 0;
				else
					goto errout;
			}
			// user namespace
			else if (strncmp(ptr, "userns ", 7) == 0) {
				if (strcmp(ptr + 7, "yes") == 0)
					cfg_val[CFG_USERNS] = 1;
				else if (strcmp(ptr + 7, "no") == 0)
					cfg_val[CFG_USERNS] = 0;
				else
					goto errout;
			}
			// chroot
			else if (strncmp(ptr, "chroot ", 7) == 0) {
				if (strcmp(ptr + 7, "yes") == 0)
					cfg_val[CFG_CHROOT] = 1;
				else if (strcmp(ptr + 7, "no") == 0)
					cfg_val[CFG_CHROOT] = 0;
				else
					goto errout;
			}
			// nonewprivs
			else if (strncmp(ptr, "force-nonewprivs ", 17) == 0) {
				if (strcmp(ptr + 17, "yes") == 0)
					cfg_val[CFG_SECCOMP] = 1;
				else if (strcmp(ptr + 17, "no") == 0)
					cfg_val[CFG_SECCOMP] = 0;
				else
					goto errout;
			}
			// seccomp
			else if (strncmp(ptr, "seccomp ", 8) == 0) {
				if (strcmp(ptr + 8, "yes") == 0)
					cfg_val[CFG_SECCOMP] = 1;
				else if (strcmp(ptr + 8, "no") == 0)
					cfg_val[CFG_SECCOMP] = 0;
				else
					goto errout;
			}
			// whitelist
			else if (strncmp(ptr, "whitelist ", 10) == 0) {
				if (strcmp(ptr + 10, "yes") == 0)
					cfg_val[CFG_WHITELIST] = 1;
				else if (strcmp(ptr + 10, "no") == 0)
					cfg_val[CFG_WHITELIST] = 0;
				else
					goto errout;
			}
			// network
			else if (strncmp(ptr, "network ", 8) == 0) {
				if (strcmp(ptr + 8, "yes") == 0)
					cfg_val[CFG_NETWORK] = 1;
				else if (strcmp(ptr + 8, "no") == 0)
					cfg_val[CFG_NETWORK] = 0;
				else
					goto errout;
			}
			// network
			else if (strncmp(ptr, "restricted-network ", 19) == 0) {
				if (strcmp(ptr + 19, "yes") == 0)
					cfg_val[CFG_RESTRICTED_NETWORK] = 1;
				else if (strcmp(ptr + 19, "no") == 0)
					cfg_val[CFG_RESTRICTED_NETWORK] = 0;
				else
					goto errout;
			}
			
			// Xephyr screen size
			else if (strncmp(ptr, "xephyr-screen ", 14) == 0) {
				// expecting two numbers and an x between them
				int n1;
				int n2;
				int rv = sscanf(ptr + 14, "%dx%d", &n1, &n2);
				if (rv != 2)
					goto errout;
				if (asprintf(&xephyr_screen, "%dx%d", n1, n2) == -1)
					errExit("asprintf");
			}
	
			// xephyr window title
			else if (strncmp(ptr, "xephyr-window-title ", 20) == 0) {
				if (strcmp(ptr + 20, "yes") == 0)
					cfg_val[CFG_XEPHYR_WINDOW_TITLE] = 1;
				else if (strcmp(ptr + 20, "no") == 0)
					cfg_val[CFG_XEPHYR_WINDOW_TITLE] = 0;
				else
					goto errout;
			}
				
			// Xephyr command extra parameters
			else if (strncmp(ptr, "xephyr-extra-params ", 19) == 0) {
				xephyr_extra_params = strdup(ptr + 19);
				if (!xephyr_extra_params)
					errExit("strdup");
			}

			else
				goto errout;

			free(ptr);
		}

		fclose(fp);
		free(fname);
		initialized = 1;
	}
	
	return cfg_val[val];
	
errout:
	fprintf(stderr, "Error: invalid line %d in firejail configuration file\n", line );
	exit(1);
}

