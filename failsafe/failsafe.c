/* SPDX-License-Identifier:	GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc. All Rights Reserved.
 *
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 *
 */

#include <common.h>
#include <malloc.h>
#include <linux/sizes.h>
#include <spi_flash.h>
#include <net/tcp.h>
#include <net/httpd.h>
#include <u-boot/md5.h>

#include "fs.h"
#include "../board/ralink/common/flash_helper.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_MT7621_5PORT_SWITCH
#define RECOVERY_FW_SIZE (SZ_8M - CONFIG_DEFAULT_NOR_KERNEL_OFFSET)
#else
#define RECOVERY_FW_SIZE UINT_MAX
#endif
#define BACKUP_ADDR ((void *)CONFIG_SYS_LOAD_ADDR)
#define HTML_UTF8 "text/html; charset=utf-8"

static u32 upload_data_id;
static const void *upload_data;
static size_t upload_size;
static int reboot_requested;
static int action_pending;
static int backup_busy;
static char maintenance_token[9];

extern int write_firmware_failsafe(size_t data_addr, uint32_t data_size);

static int output_plain_file(struct httpd_response *response,
	const char *filename)
{
	const struct fs_desc *file;
	int ret = 0;

	file = fs_find_file(filename);

	response->status = HTTP_RESP_STD;

	if (file) {
		response->data = file->data;
		response->size = file->size;
	} else {
		response->data = "Error: file not found";
		response->size = strlen(response->data);
		ret = 1;
	}

	response->info.code = 200;
	response->info.connection_close = 1;
	response->info.content_type = HTML_UTF8;

	return ret;
}

static void index_handler(enum httpd_uri_handler_status status,
	struct httpd_request *request, struct httpd_response *response)
{
	char *buf, *field;

	if (status == HTTP_CB_CLOSED) {
		free(response->session_data);
		return;
	}
	if (status != HTTP_CB_NEW || output_plain_file(response, "index.html"))
		return;
	buf = malloc(response->size + 1);
	if (!buf) {
		response->info.code = 500;
		return;
	}
	memcpy(buf, response->data, response->size);
	buf[response->size] = 0;
	field = buf;
	while ((field = strstr(field, "TTTTTTTT"))) {
		memcpy(field, maintenance_token, 8);
		field += 8;
	}
	response->session_data = buf;
	response->data = buf;
}

static void reject_request(struct httpd_response *response, unsigned int code)
{
	output_plain_file(response, "fail.html");
	response->info.code = code;
}

static void upload_handler(enum httpd_uri_handler_status status,
	struct httpd_request *request, struct httpd_response *response)
{
	struct httpd_form_value *fw;
	char *buf, *field, value[11];
	u8 digest[16];
	unsigned int i;
	static const char hex[] = "0123456789abcdef";

	if (status == HTTP_CB_CLOSED) {
		free(response->session_data);
		return;
	}
	if (status != HTTP_CB_NEW)
		return;

	if (request->method != HTTP_POST) {
		reject_request(response, 405);
		return;
	}
	if (action_pending || reboot_requested) {
		reject_request(response, 503);
		return;
	}
	upload_size = 0;
	fw = httpd_request_find_value(request, "firmware");
	/* Small requests live in the HTTP connection and cannot outlive it. */
	if (!fw || !fw->filename || fw->size < SZ_4K ||
	    fw->size > RECOVERY_FW_SIZE) {
		reject_request(response, 400);
		return;
	}
	if (output_plain_file(response, "upload.html")) {
		response->info.code = 500;
		return;
	}
	buf = malloc(response->size + 1);
	if (!buf) {
		reject_request(response, 500);
		return;
	}
	memcpy(buf, response->data, response->size);
	buf[response->size] = 0;

	md5((u8 *)fw->data, fw->size, digest);
	field = strstr(buf, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
	for (i = 0; i < sizeof(digest); i++) {
		field[2 * i] = hex[digest[i] >> 4];
		field[2 * i + 1] = hex[digest[i] & 0xf];
	}
	field = strstr(buf, "YYYYYYYYYY");
	snprintf(value, sizeof(value), "%10u", (unsigned int)fw->size);
	memcpy(field, value, 10);
	field = strstr(buf, "ZZZZZZZZ");
	snprintf(value, sizeof(value), "%08x", upload_id);
	memcpy(field, value, 8);

	response->session_data = buf;
	response->data = buf;
	upload_data_id = upload_id;
	upload_data = fw->data;
	upload_size = fw->size;
}

#ifdef CONFIG_MT7621_5PORT_SWITCH
static struct spi_flash *recovery_flash(void)
{
	struct spi_flash *flash = mtk_board_get_flash_dev();
	u64 off, size;

	if (!flash || flash->size != SZ_8M || flash->erase_size != SZ_64K ||
	    get_mtd_part_info("firmware", &off, &size) ||
	    off != CONFIG_DEFAULT_NOR_KERNEL_OFFSET || size != RECOVERY_FW_SIZE)
		return NULL;
	return flash;
}

static int recovery_buffer_available(void)
{
	return gd->start_addr_sp >= CONFIG_SYS_LOAD_ADDR + SZ_8M + SZ_1M;
}

static int restore_settings(void)
{
	struct spi_flash *flash;

	if (!recovery_buffer_available())
		return -1;
	flash = recovery_flash();
	return flash ? mt7621_restore_settings(flash, BACKUP_ADDR) : -1;
}

static void backup_handler(enum httpd_uri_handler_status status,
	struct httpd_request *request, struct httpd_response *response)
{
	struct spi_flash *flash;

	if (status == HTTP_CB_CLOSED) {
		if (response->session_data)
			backup_busy = 0;
		return;
	}
	if (status != HTTP_CB_NEW)
		return;

	if (request->method != HTTP_GET) {
		reject_request(response, 405);
		return;
	}
	if (backup_busy || action_pending || reboot_requested ||
	    !recovery_buffer_available()) {
		reject_request(response, 503);
		return;
	}
	flash = recovery_flash();
	if (!flash ||
	    mtk_board_flash_read(flash, 0, SZ_8M, SZ_8M, BACKUP_ADDR)) {
		reject_request(response, 500);
		return;
	}
	backup_busy = 1;
	response->session_data = &backup_busy;
	response->status = HTTP_RESP_STD;
	response->info.code = 200;
	response->info.connection_close = 1;
	response->info.content_type = "application/octet-stream";
	response->info.content_disposition =
		"attachment; filename=mt7621da-spi8m-backup.bin";
	response->data = BACKUP_ADDR;
	response->size = SZ_8M;
}
#endif

static int confirmed(struct httpd_request *request, int restore)
{
	struct httpd_form_value *field;

	field = httpd_request_find_value(request, "confirm");
	if (!field || field->size != 8 ||
	    memcmp(field->data, maintenance_token, 8))
		return 0;
	if (!restore)
		return 1;
	field = httpd_request_find_value(request, "erase");
	return field && field->size == 1 && field->data[0] == '1';
}

static int upload_confirmed(struct httpd_request *request)
{
	struct httpd_form_value *ticket;
	char expected[9];

	if (!upload_size || upload_data_id != upload_id)
		return 0;
	ticket = httpd_request_find_value(request, "ticket");
	snprintf(expected, sizeof(expected), "%08x", upload_data_id);
	return ticket && ticket->size == 8 &&
	       !memcmp(ticket->data, expected, 8);
}

enum maintenance_action {
	ACTION_NONE,
	ACTION_FLASH_PAGE,
	ACTION_FLASH_WRITE,
	ACTION_RESTORE,
	ACTION_REBOOT,
	ACTION_DONE
};

static void maintenance_handler(enum httpd_uri_handler_status status,
	struct httpd_request *request, struct httpd_response *response)
{
	enum maintenance_action action = (ulong)response->session_data;
	const struct fs_desc *page;
	int ret;

	if (status == HTTP_CB_CLOSED) {
		if (action)
			action_pending = 0;
		if (action == ACTION_DONE || action == ACTION_REBOOT) {
			reboot_requested = 1;
			tcp_close_all_conn();
		}
		return;
	}
	if (status == HTTP_CB_NEW) {
		static char header[128];
		const char *uri = request->urih->uri;

		if (request->method != HTTP_POST) {
			reject_request(response, 405);
			return;
		}
		if (action_pending || reboot_requested || backup_busy) {
			reject_request(response, 503);
			return;
		}

		if (!strcmp(uri, "/flashing")) {
			if (!upload_confirmed(request)) {
				reject_request(response, 400);
				return;
			}
			action = ACTION_FLASH_PAGE;
		} else {
			int reboot = !strcmp(uri, "/reboot");

			if (!confirmed(request, !reboot)) {
				reject_request(response, 400);
				return;
			}
			upload_size = 0;
			if (reboot) {
				output_plain_file(response, "success.html");
				response->session_data = (void *)ACTION_REBOOT;
				action_pending = 1;
				return;
			}
			action = ACTION_RESTORE;
		}

		response->status = HTTP_RESP_CUSTOM;
		response->info.code = 200;
		response->info.http_1_0 = 1;
		response->info.content_length = -1;
		response->info.connection_close = 1;
		response->info.content_type = HTML_UTF8;
		response->size = http_make_response_header(&response->info,
							   header,
							   sizeof(header));
		response->data = header;
		response->session_data = (void *)(ulong)action;
		action_pending = 1;
		return;
	}
	if (action == ACTION_FLASH_PAGE) {
		page = fs_find_file("flashing.html");
		response->data = page->data;
		response->size = page->size;
		response->session_data = (void *)ACTION_FLASH_WRITE;
		return;
	}

	ret = -1;
	if (action == ACTION_FLASH_WRITE) {
		if (upload_confirmed(request))
			ret = write_firmware_failsafe((size_t)upload_data,
						      upload_size);
		upload_size = 0;
#ifdef CONFIG_MT7621_5PORT_SWITCH
	} else if (action == ACTION_RESTORE) {
		ret = restore_settings();
#endif
	} else {
		response->status = HTTP_RESP_NONE;
		return;
	}

	page = fs_find_file(ret ? "fail.html" : "success.html");
	response->data = page->data;
	response->size = page->size;
	response->session_data = (void *)(ulong)(ret ? ACTION_NONE : ACTION_DONE);
	if (ret)
		action_pending = 0;
}

static void style_handler(enum httpd_uri_handler_status status,
	struct httpd_request *request,
	struct httpd_response *response)
{
	if (status == HTTP_CB_NEW) {
		output_plain_file(response, "style.css");
		response->info.content_type = "text/css";
	}
}

static void not_found_handler(enum httpd_uri_handler_status status,
	struct httpd_request *request,
	struct httpd_response *response)
{
	if (status == HTTP_CB_NEW) {
		output_plain_file(response, "404.html");
		response->info.code = 404;
	}
}

int start_web_failsafe(void)
{
	struct httpd_instance *inst;

	upload_size = 0;
	reboot_requested = 0;
	action_pending = 0;
	snprintf(maintenance_token, sizeof(maintenance_token), "%08x", rand());
#ifdef CONFIG_MT7621_5PORT_SWITCH
	backup_busy = 0;
#endif
	inst = httpd_find_instance(80);
	if (inst)
		httpd_free_instance(inst);

	inst = httpd_create_instance(80);
	if (!inst) {
		printf("Error: failed to create HTTP instance on port 80\n");
		return -1;
	}

	httpd_register_uri_handler(inst, "/", &index_handler, NULL);
	httpd_register_uri_handler(inst, "/cgi-bin/luci", &index_handler, NULL);
	httpd_register_uri_handler(inst, "/upload", &upload_handler, NULL);
	httpd_register_uri_handler(inst, "/flashing", &maintenance_handler, NULL);
#ifdef CONFIG_MT7621_5PORT_SWITCH
	httpd_register_uri_handler(inst, "/restore", &maintenance_handler, NULL);
	httpd_register_uri_handler(inst, "/backup", &backup_handler, NULL);
#endif
	httpd_register_uri_handler(inst, "/reboot", &maintenance_handler, NULL);
	httpd_register_uri_handler(inst, "/style.css", &style_handler, NULL);
	httpd_register_uri_handler(inst, "", &not_found_handler, NULL);

	net_loop(TCP);

	return 0;
}

static int do_httpd(cmd_tbl_t *cmdtp, int flag, int argc,
	char *const argv[])
{
	int ret;

	printf("\nWeb failsafe UI started\n");
	
	ret = start_web_failsafe();

	if (reboot_requested)
		do_reset(NULL, 0, 0, NULL);

	return ret;
}

U_BOOT_CMD(httpd, 1, 0, do_httpd,
	"Start failsafe HTTP server", ""
);
