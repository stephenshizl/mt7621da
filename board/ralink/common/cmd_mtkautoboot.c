// SPDX-License-Identifier:	GPL-2.0+
/*
 * Copyright (C) 2018 MediaTek Incorporation. All Rights Reserved.
 *
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#include <common.h>
#include <asm/gpio.h>

struct mtk_bootmenu_entry {
	const char *desc;
	const char *cmd;
} bootmenu_entries[] = {
	{
		.desc = "Startup system (Default)",
		.cmd = "mtkboardboot"
	}, {
		.desc = "Upgrade firmware",
		.cmd = "mtkupgrade fw"
	}, {
		.desc = "Upgrade bootloader",
		.cmd = "mtkupgrade bl"
	}, {
		.desc = "Load image",
		.cmd = "mtkload"
	}
#if defined(CONFIG_MT7621_5PORT_SWITCH) && defined(CONFIG_WEBUI_FAILSAFE)
	, {
		.desc = "Web recovery (LAN, 192.168.1.1)",
		.cmd = "setenv ipaddr 192.168.1.1; setenv netmask 255.255.255.0; httpd"
	}
#endif
};

static int do_mtkautoboot(cmd_tbl_t *cmdtp, int flag, int argc,
	char *const argv[])
{
	int i;
	char key[16];
	char val[256];
	char cmd[32];
	const char *delay_str;
	u32 delay = CONFIG_MTKAUTOBOOT_DELAY;

#if defined(CONFIG_MT7621_5PORT_SWITCH) && defined(CONFIG_WEBUI_FAILSAFE)
	if (!gpio_request(18, "reset-key")) {
		if (!gpio_direction_input(18) && gpio_get_value(18) == 0) {
			mdelay(500);
			if (gpio_get_value(18) == 0) {
				printf("Release RESET for web recovery at 192.168.1.1\n");
				/* A stuck switch must still leave a usable recovery path. */
				for (i = 0; i < 100 && gpio_get_value(18) == 0; i++) {
					gpio_set_value(0, i & 1);
					mdelay(100);
				}
				gpio_set_value(0, 1);
				gpio_free(18);
				return run_command("setenv ipaddr 192.168.1.1; "
					"setenv netmask 255.255.255.0; httpd", 0);
			}
		}
		gpio_free(18);
	}
#endif

	for (i = 0; i < ARRAY_SIZE(bootmenu_entries); i++) {
		snprintf(key, sizeof(key), "bootmenu_%d", i);
		snprintf(val, sizeof(val), "%s=%s",
			 bootmenu_entries[i].desc, bootmenu_entries[i].cmd);
		env_set(key, val);
	}

	/*
	 * Remove possibly existed `next entry` to force bootmenu command to
	 * stop processing
	 */
	snprintf(key, sizeof(key), "bootmenu_%d", i);
	env_set(key, NULL);

	delay_str = env_get("bootmenu_delay");
	if (delay_str) {
		delay = simple_strtoul(delay_str, NULL, 10);
		if (delay == 0)
			delay = 1;
	}

	snprintf(cmd, sizeof(cmd), "bootmenu %u", delay);
	run_command(cmd, 0);

	return 0;
}

U_BOOT_CMD(mtkautoboot, 1, 0, do_mtkautoboot,
	"Display MediaTek bootmenu", ""
);
