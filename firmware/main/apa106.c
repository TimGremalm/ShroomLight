#include "apa106.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <soc/rmt_struct.h>
#include <soc/dport_reg.h>
#include <driver/gpio.h>
#include <soc/gpio_sig_map.h>
#include <esp_intr_alloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <driver/rmt.h>

#define ETS_RMT_CTRL_INUM	18
#define ESP_RMT_CTRL_DISABLE	ESP_RMT_CTRL_DIABLE /* Typo in esp_intr.h */

#define DIVIDER		4 /* Above 4, timings start to deviate*/
#define DURATION	12.5 /* minimum time of a single RMT duration
				in nanoseconds based on clock */

#define PULSE_T0H	(  350 / (DURATION * DIVIDER));
#define PULSE_T1H	(  1630 / (DURATION * DIVIDER));
#define PULSE_T0L	(  1630 / (DURATION * DIVIDER));
#define PULSE_T1L	(  350 / (DURATION * DIVIDER));
#define PULSE_TRS	(50000 / (DURATION * DIVIDER));

#define MAX_PULSES	32

#define RMTCHANNEL	0

typedef union {
	struct {
		uint32_t duration0:15;
		uint32_t level0:1;
		uint32_t duration1:15;
		uint32_t level1:1;
	};
	uint32_t val;
} rmtPulsePair;

static uint8_t *apa106_buffer = NULL;
static unsigned int apa106_pos, apa106_len, apa106_half;
static xSemaphoreHandle apa106_sem = NULL;
static intr_handle_t rmt_intr_handle = NULL;
static rmtPulsePair apa106_bits[2];

void apa106_initRMTChannel(int rmtChannel) {
	RMT.apb_conf.fifo_mask = 1;  //enable memory access, instead of FIFO mode.
	RMT.apb_conf.mem_tx_wrap_en = 1; //wrap around when hitting end of buffer
	RMT.conf_ch[rmtChannel].conf0.div_cnt = DIVIDER;
	RMT.conf_ch[rmtChannel].conf0.mem_size = 1;
	RMT.conf_ch[rmtChannel].conf0.carrier_en = 0;
	RMT.conf_ch[rmtChannel].conf0.carrier_out_lv = 1;
	RMT.conf_ch[rmtChannel].conf0.mem_pd = 0;

	RMT.conf_ch[rmtChannel].conf1.rx_en = 0;
	RMT.conf_ch[rmtChannel].conf1.mem_owner = 0;
	RMT.conf_ch[rmtChannel].conf1.tx_conti_mode = 0;    //loop back mode.
	RMT.conf_ch[rmtChannel].conf1.ref_always_on = 1;    // use apb clock: 80M
	RMT.conf_ch[rmtChannel].conf1.idle_out_en = 1;
	RMT.conf_ch[rmtChannel].conf1.idle_out_lv = 0;

	return;
}

void apa106_copy() {
	unsigned int i, j, offset, len, bit;

	offset = apa106_half * MAX_PULSES;
	apa106_half = !apa106_half;

	len = apa106_len - apa106_pos;
	if (len > (MAX_PULSES / 8))
	len = (MAX_PULSES / 8);

	if (!len) {
		for (i = 0; i < MAX_PULSES; i++) {
			RMTMEM.chan[RMTCHANNEL].data32[i + offset].val = 0;
		}
		return;
	}

	for (i = 0; i < len; i++) {
		bit = apa106_buffer[i + apa106_pos];
		for (j = 0; j < 8; j++, bit <<= 1) {
			RMTMEM.chan[RMTCHANNEL].data32[j + i * 8 + offset].val =
			apa106_bits[(bit >> 7) & 0x01].val;
		}
		if (i + apa106_pos == apa106_len - 1) {
			RMTMEM.chan[RMTCHANNEL].data32[7 + i * 8 + offset].duration1 = PULSE_TRS;
		}
	}

	for (i *= 8; i < MAX_PULSES; i++) {
		RMTMEM.chan[RMTCHANNEL].data32[i + offset].val = 0;
	}

	apa106_pos += len;
	return;
}

void apa106_handleInterrupt(void *arg) {
	portBASE_TYPE taskAwoken = 0;

	if (RMT.int_st.ch0_tx_thr_event) {
		apa106_copy();
		RMT.int_clr.ch0_tx_thr_event = 1;
	} else if (RMT.int_st.ch0_tx_end && apa106_sem) {
		xSemaphoreGiveFromISR(apa106_sem, &taskAwoken);
		RMT.int_clr.ch0_tx_end = 1;
	}

	return;
}

void apa106_init(int gpioNum) {
	DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
	DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

	rmt_set_pin((rmt_channel_t)RMTCHANNEL, RMT_MODE_TX, (gpio_num_t)gpioNum);

	apa106_initRMTChannel(RMTCHANNEL);

	RMT.tx_lim_ch[RMTCHANNEL].limit = MAX_PULSES;
	RMT.int_ena.ch0_tx_thr_event = 1;
	RMT.int_ena.ch0_tx_end = 1;

	apa106_bits[0].level0 = 1;
	apa106_bits[0].level1 = 0;
	apa106_bits[0].duration0 = PULSE_T0H;
	apa106_bits[0].duration1 = PULSE_T0L;
	apa106_bits[1].level0 = 1;
	apa106_bits[1].level1 = 0;
	apa106_bits[1].duration0 = PULSE_T1H;
	apa106_bits[1].duration1 = PULSE_T1L;

	esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, apa106_handleInterrupt, NULL, &rmt_intr_handle);
	return;
}

void apa106_setColors(unsigned int length, rgbVal *array) {
	unsigned int i;

	apa106_len = (length * 3) * sizeof(uint8_t);
	apa106_buffer = malloc(apa106_len);

	for (i = 0; i < length; i++) {
		apa106_buffer[0 + i * 3] = array[i].r;
		apa106_buffer[1 + i * 3] = array[i].g;
		apa106_buffer[2 + i * 3] = array[i].b;
	}

	apa106_pos = 0;
	apa106_half = 0;

	apa106_copy();

	if (apa106_pos < apa106_len) {
		apa106_copy();
	}

	apa106_sem = xSemaphoreCreateBinary();

	//xt_ints_off(1 << ETS_WMAC_INUM);

	RMT.conf_ch[RMTCHANNEL].conf1.mem_rd_rst = 1;
	RMT.conf_ch[RMTCHANNEL].conf1.tx_start = 1;

	xSemaphoreTake(apa106_sem, portMAX_DELAY);
	vSemaphoreDelete(apa106_sem);
	apa106_sem = NULL;

	//xt_ints_on(1 << ETS_WMAC_INUM);

	free(apa106_buffer);

	return;
}

rgbVal apa106_HsvToRgb(hsvVal hsv) {
	rgbVal rgb;
	uint8_t region, remainder, p, q, t;

	if (hsv.s == 0) {
		rgb.r = hsv.v;
		rgb.g = hsv.v;
		rgb.b = hsv.v;
		return rgb;
	}

	region = hsv.h / 43;
	remainder = (hsv.h - (region * 43)) * 6;

	p = (hsv.v * (255 - hsv.s)) >> 8;
	q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
	t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

	switch (region) {
		case 0:
			rgb.r = hsv.v; rgb.g = t; rgb.b = p;
			break;
		case 1:
			rgb.r = q; rgb.g = hsv.v; rgb.b = p;
			break;
		case 2:
			rgb.r = p; rgb.g = hsv.v; rgb.b = t;
			break;
		case 3:
			rgb.r = p; rgb.g = q; rgb.b = hsv.v;
			break;
		case 4:
			rgb.r = t; rgb.g = p; rgb.b = hsv.v;
			break;
		default:
			rgb.r = hsv.v; rgb.g = p; rgb.b = q;
			break;
	}

	return rgb;
}

hsvVal apa106_RgbToHsv(rgbVal rgb) {
	hsvVal hsv;
	uint8_t rgbMin, rgbMax;

	rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
	rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

	hsv.v = rgbMax;
	if (hsv.v == 0) {
		hsv.h = 0;
		hsv.s = 0;
		return hsv;
	}

	hsv.s = 255 * (uint16_t)(rgbMax - rgbMin) / hsv.v;
	if (hsv.s == 0) {
		hsv.h = 0;
		return hsv;
	}

	if (rgbMax == rgb.r) {
		hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
	} else if (rgbMax == rgb.g) {
		hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
	} else {
		hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);
	}

	return hsv;
}

