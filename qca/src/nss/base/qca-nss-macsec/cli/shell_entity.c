/*
 * Copyright (c) 2014, 2019 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <vty.h>
#include <cli.h>

VTY_T *vty_entity = NULL;

static int _shell_init()
{

	SHELL_RET_ON_ERR(cli_init());

	vty_entity = vty_create(fileno(stdout));
	if (vty_entity == NULL) {
		return -1;
	}

	return 0;
}

static int _shell_fini(void)
{
	vty_close(vty_entity);

	vty_entity = NULL;	/* MUST: avoid re-enter shell SEGAMENT FAULT */

	SHELL_RET_ON_ERR(cli_exit());

	return 0;
}

int shell_loop()
{
	SHELL_RET_ON_ERR(_shell_init());

	while (vty_entity->state != VTY_STATE_CLOSE) {
		char cc = getc(stdin);

		vty_input(vty_entity, (sa_u8_t *)&cc, 1);
	}

	SHELL_RET_ON_ERR(_shell_fini());

	return 0;
}

int shell_write(int sock, char *buf, int nbytes)
{
	write(sock, buf, nbytes);

	return 0;
}

static int __shell_cat(const char *file)
{
	FILE *fp;
	char fp_buffer[128];

	fp = fopen(file, "r");

	if (fp) {
		while (fgets(fp_buffer, 127, fp)) {
			osal_print("%s", fp_buffer);
		}
		fclose(fp);
		osal_print("\n");
	}

	return 0;
}

int shell_system(char *command)
{
	if (strncmp(command, "cat ", 4) == 0) {
		char file[128] = { 0 };
		sscanf(command, "cat %127s", file);
		__shell_cat(file);
	} else {
		system(command);
	}

	return 0;
}
