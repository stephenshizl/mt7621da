/*
 * Control BLE Advertising
 *
 */

#include <dm.h>
#include <errno.h>
#include <serial.h>
#include <stdio.h>
#include <asm/addrspace.h>
#include <asm/gpio.h>

#define MT7621_UART3_BASE				0x1e000e00

#define CMD_RET_SUCCESS					1
#define CMD_RET_FAIL					0

#define TRUE						1
#define FALSE						0

#define BLE_FORMAT_HEADER_LEN				4

#define BLE_PW_ON_IDX					0
#define BLE_HCI_RESET_IDX				1
#define BLE_HCI_SET_ADV_PARAM_IDX			2
#define BLE_HCI_SET_ADV_DATA_IDX			3
#define BLE_HCI_SET_SCAN_RESP_DATA_IDX			4
#define BLE_HCI_SET_ADV_ENABLE_IDX			5
#define BLE_HCI_SET_BT_MAC_IDX				6
#define BLE_HCI_SET_BT_HIGH_RATE_IDX			7
#define BLE_HCI_SET_BT_HIGH_RATE_SW_IDX			8

#define GPIO_CLEAR					0
#define GPIO_SET					1

#define STP_ENABLE					0
#define STP_HEAD_FORMAT_LEN				4
#define STP_TAIL_FORMAT_LEN				2

#define HIGH_BAUD_RATE					921600
#define HIGH_BAUD_RATE_ENABLE				0

#define TX_BYTE_SIZE					50
#define RX_BYTE_SIZE					40

/* STP format */
static u8 stp_head_tx_pw_on_cmd[] = {0x80, 0x00, 0x0A, 0x00};
static u8 stp_head_tx_hci_reset_cmd[] = {0x80, 0x00, 0x04, 0x00};
static u8 stp_head_tx_adv_param_cmd[] = {0x80, 0x00, 0x13, 0x00};
static u8 stp_head_tx_adv_data_cmd[] = {0x80, 0x00, 0x24, 0x00};
static u8 stp_head_tx_scan_rsp_data_cmd[] = {0x80, 0x00, 0x24, 0x00};
static u8 stp_head_tx_adv_en_cmd[] = {0x80, 0x00, 0x05, 0x00};
static u8 stp_head_tx_bt_mac_cmd[] = {0x80, 0x00, 0x0A, 0x00};
static u8 stp_head_tx_bt_baud_rate_921600_cmd[] = {0x80, 0x00, 0x09, 0x00};
static u8 stp_head_tx_bt_baud_rate_switch_cmd[] = {0x80, 0x00, 0x05, 0x00};
static u8 stp_tail_txrx_cmd[] = {0x00, 0x00};

/* BLE EXPECTED RX Messgae */
static u8 pw_on_rx_code[] = {
	0x04, 0xe4, 0x05, 0x02, 0x06,
	0x01, 0x00, 0x00
};

static u8 hic_reset_rx_code[] = {
	0x04, 0x0E, 0x04, 0x01, 0x03,
	0x0C, 0x00
};

static u8 set_adv_param_rx_code[] = {
	0x04, 0x0E, 0x04, 0x01, 0x06,
	0x20, 0x00
};

static u8 set_adv_data_rx_code[] = {
	0x04, 0x0E, 0x04, 0x01, 0x08,
	0x20, 0x00
};

static u8 set_scan_resp_data_rx_code[] = {
	0x04, 0x0E, 0x04, 0x01, 0x09,
	0x20, 0x00
};

static u8 set_adv_en_rx_code[] = {
	0x04, 0x0E, 0x04, 0x01, 0x0A,
	0x20, 0x00
};

static u8 set_bt_mac_rx_code[] = {
	0x04, 0x0E, 0x04, 0x01, 0x1A,
	0xFC, 0x00
};

static u8 set_baud_rate_rx_code[] = {
	0x04, 0xe4, 0x06, 0x02, 0x04,
	0x02, 0x00, 0x00, 0x01
};

static u8 baud_rate_sw_rx_code[] = {
	0x04, 0xe4, 0x06, 0x02, 0x03,
	0x02, 0x00, 0x00, 0x03
};

/* BLE TX Command */
static u8 bt_power_on_tx_cmd[] = {
	0x01, 0x6f, 0xfc, 0x06, 0x01,
	0x06, 0x02, 0x00, 0x00, 0x01
};

static u8 hci_reset_tx_cmd[] = {
	0x01, 0x03, 0x0C, 0x00
};

static u8 hci_le_set_adv_param_tx_cmd[] = {
	0x01, 0x06, 0x20, 0x0f,
	0xa0, 0x00,
	/* Advertising Interval Min (x0.625ms) */
	0xa0, 0x00,
	/* Advertising Interval Max (x0.625ms) */
	0x02,
	/* Advertising Type */
	0x00,
	/* Own Address Type */
	0x00,
	/* Peer Address Type */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* Peer Address */
	0x07,
	/* Advertising Channel Map */
	0x00
	/* Advertising Filter Policy */
};

/* Sample1: MT7915_BLE_ADV_TEST */
static u8 hci_le_set_adv_data_tx_cmd[] = {
	0x01, 0x08, 0x20, 0x20,
	0x18,
	/* Advertising Data Length, Total 31 bytes */
	0x02, 0x01, 0x1A, 0x14, 0x09, 0x4d,
	/* Advertising Data Byte0~Byte6 */
	0x54, 0x37, 0x39, 0x31, 0x35, 0x5f,
	/* Advertising Data Byte7~Byte12 */
	0x42, 0x4c, 0x45, 0x5f, 0x41, 0x44,
	/* Advertising Data Byte13~Byte18 */
	0x56, 0x5f, 0x54, 0x45, 0x53, 0x54,
	/* Advertising Data Byte19~Byte24 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* Advertising Data Byte25~Byte30 */
	0x00
	/* Advertising Data Byte31 */
};

static u8 hci_le_set_scan_resp_data_tx_cmd[] = {
	0x01, 0x09, 0x20, 0x20,
	0x18,
	/* Scan Response Data Length, Total 31 bytes */
	0x02, 0x01, 0x1A, 0x14, 0x09, 0x4d,
	/* Scan Response Data Byte0~Byte6 */
	0x54, 0x37, 0x39, 0x31, 0x35, 0x5f,
	/* Scan Response Data Byte7~Byte12 */
	0x42, 0x4c, 0x45, 0x5f, 0x41, 0x44,
	/* Scan Response Data Byte13~Byte18 */
	0x56, 0x5f, 0x54, 0x45, 0x53, 0x54,
	/* Scan Response Data Byte19~Byte24 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* Scan Response Data Byte25~Byte30 */
	0x00
	/* Scan Response Data Byte31 */
};

/* Advertising Enable/Disable */
static u8 hci_le_set_adv_enable_tx_cmd[] = {
	0x01, 0x0A, 0x20, 0x01, 0x01
};

/* BT MAC Setting, ex:11:22:33:44:55:66 */
static u8 hci_le_set_bt_mac_tx_cmd[] = {
	0x01, 0x1A, 0xFC, 0x06, 0x66,
	0x55, 0x44, 0x33, 0x22, 0x11
};

static u8  hci_le_set_bt_baud_rate_921600_tx_cmd[] = {
	0x01, 0x6f, 0xfc, 0x09, 0x01,
	0x04, 0x05, 0x00, 0x01, 0x00,
	0x10, 0x0e, 0x00
};

static u8  hci_le_set_bt_baud_rate_switch_tx_cmd[] = {
	0x01, 0x6f, 0xfc, 0x01, 0xff
};

static void _serial_putc(struct udevice *dev, char ch)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int err;

	do {
		err = ops->putc(dev, ch);
	} while (err == -EAGAIN);
}

static int _serial_getc(struct udevice *dev)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int err;

	do {
		err = ops->getc(dev);
	} while (err == -EAGAIN);

	return err >= 0 ? err : 0;
}

static int _serial_tstc(struct udevice *dev)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);

	if (ops->pending)
		return ops->pending(dev, true);

	return 1;
}

static void _serial_setbrg(struct udevice *dev, int baudrate)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);

	if (ops->setbrg)
		ops->setbrg(dev, baudrate);
}

static int get_ble_tx_cmd(u8 idx, u8 tx_cmd_buf[])
{
	u8 len = 0;
	u8 stp_len;
	u8 *s = NULL;
	u8 *d = NULL;
	u8 l = 0;

	memset(tx_cmd_buf, 0, TX_BYTE_SIZE);
	stp_len = STP_HEAD_FORMAT_LEN;

	switch (idx) {
	case BLE_PW_ON_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_pw_on_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = bt_power_on_tx_cmd;
		l = sizeof(bt_power_on_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_RESET_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_hci_reset_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_reset_tx_cmd;
		l = sizeof(hci_reset_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_SET_ADV_PARAM_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_adv_param_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_le_set_adv_param_tx_cmd;
		l = sizeof(hci_le_set_adv_param_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_SET_ADV_DATA_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_adv_data_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_le_set_adv_data_tx_cmd;
		l = sizeof(hci_le_set_adv_data_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_SET_SCAN_RESP_DATA_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_scan_rsp_data_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_le_set_scan_resp_data_tx_cmd;
		l = sizeof(hci_le_set_scan_resp_data_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_SET_ADV_ENABLE_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_adv_en_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_le_set_adv_enable_tx_cmd;
		l = sizeof(hci_le_set_adv_enable_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_SET_BT_MAC_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_bt_mac_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_le_set_bt_mac_tx_cmd;
		l = sizeof(hci_le_set_bt_mac_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_SET_BT_HIGH_RATE_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_bt_baud_rate_921600_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_le_set_bt_baud_rate_921600_tx_cmd;
		l = sizeof(hci_le_set_bt_baud_rate_921600_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	case BLE_HCI_SET_BT_HIGH_RATE_SW_IDX:
		if (STP_ENABLE) {
			s = tx_cmd_buf;
			d = stp_head_tx_bt_baud_rate_switch_cmd;
			l = stp_len;
			memcpy(s, d, l);
			len = stp_len;
		}

		s = &tx_cmd_buf[len];
		d = hci_le_set_bt_baud_rate_switch_tx_cmd;
		l = sizeof(hci_le_set_bt_baud_rate_switch_tx_cmd);
		memcpy(s, d, l);
		len = len + l;
		break;

	default:
		break;
	}

	if (STP_ENABLE) {
		s = &tx_cmd_buf[len];
		d = stp_tail_txrx_cmd;
		l = sizeof(stp_tail_txrx_cmd);
		memcpy(s, d, l);
		len = len + STP_TAIL_FORMAT_LEN;
	}

	return len;
}

static int judge_rx_msg(u8 msg_idx, u8 rx_msg_buf[])
{
	u8 ret = 0;
	u8 stp_len = 0;
	u8 *s = NULL;
	u8 *d = NULL;
	u8 l = 0;

	stp_len = STP_HEAD_FORMAT_LEN;

	switch (msg_idx) {
	case BLE_PW_ON_IDX:

		d = pw_on_rx_code;
		l = sizeof(pw_on_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	case BLE_HCI_RESET_IDX:

		d = hic_reset_rx_code;
		l = sizeof(hic_reset_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	case BLE_HCI_SET_ADV_PARAM_IDX:

		d = set_adv_param_rx_code;
		l = sizeof(set_adv_param_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	case BLE_HCI_SET_ADV_DATA_IDX:

		d = set_adv_data_rx_code;
		l = sizeof(set_adv_data_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	case BLE_HCI_SET_SCAN_RESP_DATA_IDX:

		d = set_scan_resp_data_rx_code;
		l = sizeof(set_scan_resp_data_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	case BLE_HCI_SET_ADV_ENABLE_IDX:

		d = set_adv_en_rx_code;
		l = sizeof(set_adv_en_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	case BLE_HCI_SET_BT_HIGH_RATE_IDX:

		d = set_baud_rate_rx_code;
		l = sizeof(set_baud_rate_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	case BLE_HCI_SET_BT_HIGH_RATE_SW_IDX:

		d = baud_rate_sw_rx_code;
		l = sizeof(baud_rate_sw_rx_code);

		if (STP_ENABLE)
			s = &rx_msg_buf[stp_len];
		else
			s = rx_msg_buf;

		debug("[judge_rx_msg]: l=%d, ret=%d\n", l, ret);
		ret = memcmp(s, d, l);
		break;

	default:
		break;
	}

	if (ret == 0)
		return CMD_RET_SUCCESS;

	return CMD_RET_FAIL;
}

static int do_mtk_bleadv(cmd_tbl_t *cmdtp, int flag, int argc,	char *const argv[])
{
	/* START BT PROCESS */
	struct udevice *uart;
	u8 bt_tx_cmd[TX_BYTE_SIZE] = { 0 };
	u8 bt_rx_msg[RX_BYTE_SIZE] = { 0 };
	u8 num = 0;
	u8 len = 0;
	u8 i = 0;
	u8 ch = 0;
	u8 idx = 0;
	u8 curr_step_pass = 0;
	u8 baud_rate_switch = 0;
	u8 count = 4;
	u32 gpio;
	u32 reg;
	u32 start_timep;
	u32 curr_timep;
	int ret;
	int value;
	char *str_gpio = NULL;
	char *str_gpio_action = NULL;

	debug("=========================GPIO INIT==================\n");

	while (count) {
		if (count == 4) {
			value = GPIO_CLEAR;
			str_gpio = "16";
			str_gpio_action = "output low";
		} else if (count == 3) {
			value = GPIO_SET;
			str_gpio = "16";
			str_gpio_action = "output high";
		} else if (count == 2) {
			value = GPIO_CLEAR;
			str_gpio = "19";
			str_gpio_action = "output low";
		} else if (count == 1) {
			value = GPIO_SET;
			str_gpio = "19";
			str_gpio_action = "output high";
		} else
			return -1;

		ret = gpio_lookup_name(str_gpio, NULL, NULL, &gpio);
		if (ret) {
			printf("GPIO: '%s' not found\n", str_gpio);
			return -1;
		}

		ret = gpio_request(gpio, "cmd_gpio");
		if (ret && ret != -EBUSY) {
			printf("gpio: requesting pin %u failed\n", gpio);
			return -1;
		}

		debug("GPIO_%s, action is %s", str_gpio, str_gpio_action);
		gpio_direction_output(gpio, value);

		count--;
		mdelay(10);
	}

	debug("\n=========================UART_3 INIT================\n");
	ret = uclass_first_device_check(UCLASS_SERIAL, &uart);
	if (!uart) {
		printf("No valid serial device found\n");
		return CMD_RET_FAILURE;
	}

	while (uart) {
		if (ret >= 0) {
			reg = CPHYSADDR(ofnode_get_addr(uart->node));
			debug("%s, %08x\n", uart->name, reg);

			if (reg == MT7621_UART3_BASE) {
				debug("%s bring up, %08x", uart->name, reg);
				break;
			} else
				debug("%s, %08x\n", uart->name, reg);
		}
		ret = uclass_next_device_check(&uart);
	}

	if (!uart) {
		printf("UART3 device not found\n");
		return CMD_RET_FAILURE;
	}

	_serial_setbrg(uart, 115200);
	mdelay(10);

	/* Extend FLOW: BLE_HCI_SET_BT_HIGH_RATE_921600 */
	debug("\n=========================Extend FLOW================\n");
	if (HIGH_BAUD_RATE_ENABLE) {
		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_SET_BT_HIGH_RATE_IDX;
		len = 0;

		if (STP_ENABLE) {
			len += sizeof(set_baud_rate_rx_code);
			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else {
			len = sizeof(set_baud_rate_rx_code);
		}

		ret = get_ble_tx_cmd(idx, bt_tx_cmd);

		if (ret > 0) {
			num = ret;
			debug("[BT Baud Rate 921600]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		printf("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[BT Baud Rate 921600]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_SET_BT_HIGH_RATE_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[BT Baud Rate 921600 Setting Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success");
			baud_rate_switch = 1;
		} else {
			debug(" Fail");
		}
	} else {
		debug("[BT Baud Rate is default Setting: 115200]");
	}

	/* Extend FLOW: BLE_HCI_SET_BT_HIGH_RATE_SWITCH */
	debug("\n=========================Extend FLOW================\n");
	if (baud_rate_switch == 1) {
		_serial_setbrg(uart, 921600);

		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_SET_BT_HIGH_RATE_SW_IDX;
		len = 0;
		if (STP_ENABLE) {
			len += sizeof(baud_rate_sw_rx_code);
			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else {
			len += sizeof(baud_rate_sw_rx_code);
		}

		ret = get_ble_tx_cmd(idx, bt_tx_cmd);

		if (ret > 0) {
			num = ret;
			debug("[BT Baud Rate SWITCH]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		debug("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[BT Baud Rate SWITCH]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_SET_BT_HIGH_RATE_SW_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[BT Baud Rate Switch Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success");
		} else {
			debug(" Fail");
		}
	} else {
		debug("Nothing to do");
	}

	/* FLOW 1: BT Power On */
	debug("\n=========================FLOW 1=====================\n");
	memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
	idx = BLE_PW_ON_IDX;
	len = 0;

	if (STP_ENABLE) {
		len += sizeof(pw_on_rx_code);
		len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
	} else
		len = sizeof(pw_on_rx_code);

	ret = get_ble_tx_cmd(idx, bt_tx_cmd);

	if (ret > 0) {
		num = ret;
		debug("[BT Power On]: Tx Cmd=");
		for (i = 0; i < num; i++) {
			_serial_putc(uart, bt_tx_cmd[i]);
			debug("%02x ", bt_tx_cmd[i]);
		}
		debug("\n");
	}
	debug("len= %d, num= %d\n", len, num);

	i = 0;
	memset(bt_rx_msg, 0, RX_BYTE_SIZE);
	debug("[BT Power On]: Rx Msg=");
	start_timep = get_timer(0);

	while (i < len) {
		if (_serial_tstc(uart)) {
			ch = _serial_getc(uart);
			memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
			debug("%02x ", bt_rx_msg[i]);
			i++;
		}
		curr_timep = get_timer(0);
		if (curr_timep - start_timep > 1500)
			break;
	}
	debug("\n");

	idx = BLE_PW_ON_IDX;
	ret = judge_rx_msg(idx, bt_rx_msg);
	debug("[BT Power On Result]");

	if (ret == CMD_RET_SUCCESS) {
		curr_step_pass = TRUE;
		debug(" Success");
	} else {
		debug(" Fail");
	}

	/* FLOW 2: HCI_RESET */
	debug("\n=========================FLOW 2=====================\n");
	if (curr_step_pass == TRUE) {
		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_RESET_IDX;
		len = 0;
		if (STP_ENABLE) {
			len += sizeof(hic_reset_rx_code);

			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else
			len = sizeof(hic_reset_rx_code);

		ret = get_ble_tx_cmd(idx, bt_tx_cmd);

		if (ret > 0) {
			num = ret;
			debug("[HCI RESET]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		debug("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[HCI RESET]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_RESET_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[HIC RESET Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success");
		} else {
			curr_step_pass = FALSE;
			debug(" Fail");
		}
	}

	/* Extend FLOW: HCI_LE_SET_BT_MAC_ADDR */
	debug("\n=========================Extend FLOW================\n");
	if (curr_step_pass == TRUE) {
		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_SET_BT_MAC_IDX;
		len = 0;

		if (STP_ENABLE) {
			len += sizeof(set_bt_mac_rx_code);
			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else
			len = sizeof(set_bt_mac_rx_code);

		ret = get_ble_tx_cmd(idx, bt_tx_cmd);

		if (ret > 0) {
			num = ret;
			debug("[HCI LE_Set BT MAC ADDR]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		debug("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[HCI LE_Set BT MAC ADDR]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_SET_BT_MAC_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[HIC LE  BT MAC ADDR Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success");
		} else {
			curr_step_pass = FALSE;
			debug(" Fail");
		}
	}

	/* FLOW 3: HCI_LE_Set_Advertising_Parameters */
	debug("\n=========================FLOW 3=====================\n");
	if (curr_step_pass == TRUE) {
		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_SET_ADV_PARAM_IDX;
		len = 0;

		if (STP_ENABLE) {
			len += sizeof(set_adv_param_rx_code);
			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else {
			len += sizeof(set_adv_param_rx_code);

		}

		ret = get_ble_tx_cmd(idx, bt_tx_cmd);

		if (ret > 0) {
			num = ret;
			debug("[HCI LE_Set Advertising Parameters]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		debug("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[HCI LE_Set Advertising Parameters]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_SET_ADV_PARAM_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[HIC LE  SET ADVERTISING PARAMETER Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success");
		} else {
			curr_step_pass = FALSE;
			debug(" Fail");
		}
	}

	/* FLOW 4: HCI_LE_Set_Advertising_Data */
	debug("\n=========================FLOW 4=====================\n");
	if (curr_step_pass == TRUE) {
		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_SET_ADV_DATA_IDX;
		len = 0;

		if (STP_ENABLE) {
			len += sizeof(set_adv_data_rx_code);
			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else {
			len += sizeof(set_adv_data_rx_code);
		}

		ret = get_ble_tx_cmd(idx, bt_tx_cmd);
		if (ret > 0) {
			num = ret;
			debug("[HCI LE_Set Advertising Data]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		debug("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[HCI LE_Set Advertising Data]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_SET_ADV_DATA_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[HIC LE  SET ADVERTISING DATA Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success");
		} else {
			curr_step_pass = FALSE;
			debug(" Fail");
		}
	}

	/* FLOW 5: HCI_LE_Set_Scan_Response_Data */
	debug("\n=========================FLOW 5=====================\n");
	if (curr_step_pass == TRUE) {
		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_SET_SCAN_RESP_DATA_IDX;
		len = 0;

		if (STP_ENABLE) {
			len += sizeof(set_scan_resp_data_rx_code);
			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else {
			len += sizeof(set_scan_resp_data_rx_code);
		}

		ret = get_ble_tx_cmd(BLE_HCI_SET_SCAN_RESP_DATA_IDX, bt_tx_cmd);

		if (ret > 0) {
			num = ret;
			debug("[HCI LE_Set Scan Response Data]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		debug("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[HCI LE_Set Scan Response Data ]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_SET_SCAN_RESP_DATA_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[HIC LE  SET SCAN RESPONSE Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success");
		} else {
			curr_step_pass = FALSE;
			debug(" Fail");
		}
	}

	/* FLOW 7: HCI_LE_Set_Advertising_Enable */
	debug("\n=========================FLOW 6=====================\n");
	if (curr_step_pass == TRUE) {
		memset(bt_tx_cmd, 0, TX_BYTE_SIZE);
		idx = BLE_HCI_SET_ADV_ENABLE_IDX;
		len = 0;

		if (STP_ENABLE) {
			len += sizeof(set_adv_en_rx_code);
			len += STP_HEAD_FORMAT_LEN + STP_TAIL_FORMAT_LEN;
		} else
			len = sizeof(set_adv_en_rx_code);

		ret = get_ble_tx_cmd(idx, bt_tx_cmd);

		if (ret > 0) {
			num = ret;
			debug("[HCI LE_Set Advertising Enable]: Tx Cmd=");
			for (i = 0; i < num; i++) {
				_serial_putc(uart, bt_tx_cmd[i]);
				mdelay(1);
				debug("%02x ", bt_tx_cmd[i]);
			}
			debug("\n");
		}
		debug("len= %d, num= %d\n", len, num);

		i = 0;
		memset(bt_rx_msg, 0, RX_BYTE_SIZE);
		debug("[HCI LE_Set Advertising Enable]: Rx Msg=");
		start_timep = get_timer(0);

		while (i < len) {
			if (_serial_tstc(uart)) {
				ch = _serial_getc(uart);
				memcpy(&bt_rx_msg[i], &ch, sizeof(ch));
				debug("%02x ", bt_rx_msg[i]);
				i++;
				mdelay(1);
			}
			curr_timep = get_timer(0);
			if (curr_timep - start_timep > 1500)
				break;
		}
		debug("\n");

		idx = BLE_HCI_SET_ADV_ENABLE_IDX;
		ret = judge_rx_msg(idx, bt_rx_msg);
		debug("[HIC LE  SET ADVERISTING ENABLE Result]");

		if (ret == CMD_RET_SUCCESS) {
			curr_step_pass = TRUE;
			debug(" Success\n");
			printf("ADV BNC Flow Done\n");
		} else {
			curr_step_pass = FALSE;
			debug(" Fail\n");
			printf("ADV BNC Flow Error\n");
		}
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(mtk_bleadv, 1, 0, do_mtk_bleadv, "Test MTK BLE Advertising",
	   "This feature is based on uart3 function enable");
