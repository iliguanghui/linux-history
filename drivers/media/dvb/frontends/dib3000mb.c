/*
 * Frontend driver for mobile DVB-T demodulator DiBcom 3000-MB
 * DiBcom (http://www.dibcom.fr/)
 *
 * Copyright (C) 2004 Patrick Boettcher (patrick.boettcher@desy.de)
 *
 * based on GPL code from DibCom, which has
 *
 * Copyright (C) 2004 Amaury Demol for DiBcom (ademol@dibcom.fr)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 * Acknowledgements
 *
 *  Amaury Demol (ademol@dibcom.fr) from DiBcom for providing specs and driver
 *  sources, on which this driver (and the dvb-dibusb) are based.
 *
 * 
 * 
 * see Documentation/dvb/README.dibusb for more information
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>

#include "dvb_frontend.h"

#include "dib3000mb.h"

/* debug */

#ifdef CONFIG_DVB_DIBCOM_DEBUG
#define dprintk(level,args...) \
	do { if ((debug & level)) { printk(args); } } while (0)

static int debug;
module_param(debug, int, 0x644);
MODULE_PARM_DESC(debug, "set debugging level (1=info,2=xfer,4=alotmore,8=setfe,16=getfe (|-able)).");
#else
#define dprintk(args...) do { } while (0);
#endif

#define deb_info(args...) dprintk(0x01,args)
#define deb_xfer(args...) dprintk(0x02,args)
#define deb_alot(args...) dprintk(0x04,args)
#define deb_setf(args...) dprintk(0x08,args)
#define deb_getf(args...) dprintk(0x10,args)

/* Version information */
#define DRIVER_VERSION "0.1"
#define DRIVER_DESC "DiBcom 3000-MB DVB-T frontend"
#define DRIVER_AUTHOR "Patrick Boettcher, patrick.boettcher@desy.de"

struct dib3000mb_state {
	struct i2c_client *i2c;
	struct dvb_adapter *dvb;
	u16 manufactor_id;
	u16 device_id;
};

static struct dvb_frontend_info dib3000mb_info = {
	.name			= "DiBcom 3000-MB DVB-T",
	.type 			= FE_OFDM,
	.frequency_min 		= 44250000,
	.frequency_max 		= 867250000,
	.frequency_stepsize	= 62500,
	.caps = FE_CAN_INVERSION_AUTO |
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO | 
			FE_CAN_HIERARCHY_AUTO,
};


#define rd(reg) dib3000mb_read_reg(state->i2c,reg)
#define wr(reg,val) if (dib3000mb_write_reg(state->i2c,reg,val)) \
	{ err("while sending 0x%04x to 0x%04x.",val,reg); return -EREMOTEIO; }
#define wr_foreach(a,v) { int i; \
	deb_alot("sizeof: %d %d\n",sizeof(a),sizeof(v));\
	for (i=0; i < sizeof(a)/sizeof(u16); i++) \
		wr(a[i],v[i]); \
}

static u16 dib3000mb_read_reg(struct i2c_client *i2c, u16 reg)
{
	u8 wb[] = { ((reg >> 8) | 0x80) & 0xff, reg & 0xff };
	u8 rb[2];
	struct i2c_msg msg[] = {
		{ .addr = i2c->addr, .flags = 0,        .buf = wb, .len = 2 },
		{ .addr = i2c->addr, .flags = I2C_M_RD, .buf = rb, .len = 2 },
	};
	deb_alot("reading from i2c bus (reg: %d)\n",reg);

	if (i2c_transfer(i2c->adapter,msg,2) != 2)
		deb_alot("i2c read error\n");

	return (rb[0] << 8) | rb[1];
}

static int dib3000mb_write_reg(struct i2c_client *i2c,u16 reg, u16 val)
{
	u8 b[] = {
		(reg >> 8) & 0xff, reg & 0xff,
		(val >> 8) & 0xff, val & 0xff,
	};
	struct i2c_msg msg[] = { { .addr = i2c->addr, .flags = 0, .buf = b, .len = 4 } };
	deb_alot("writing to i2c bus (reg: %d, val: %d)\n",reg,val);

	return i2c_transfer(i2c->adapter,msg,1) != 1 ? -EREMOTEIO : 0 ;
}

static int dib3000mb_tuner_thomson_cable_eu(struct dib3000mb_state *state,
		u32 freq)
{
	u32 tfreq = (freq + 36125000) / 62500;
	unsigned int addr;
	int vu,p0,p1,p2;

	if (freq > 403250000)
		vu = 1, p2 = 1, p1 = 0, p0 = 1;
	else if (freq > 115750000)
		vu = 0, p2 = 1, p1 = 1, p0 = 0;
	else if (freq > 44250000)
		vu = 0, p2 = 0, p1 = 1, p0 = 1;
	else
		return -EINVAL;
	/* TODO better solution for i2c->addr handling */
	addr = state->i2c->addr;
	state->i2c->addr = DIB3000MB_TUNER_ADDR_DEFAULT;
	wr(tfreq & 0x7fff,(0x8e << 8) + ((vu << 7) | (p2 << 2) | (p1 << 1) | p0) );
	state->i2c->addr = addr;

	return 0;
}

static int dib3000mb_get_frontend(struct dib3000mb_state *state,
		struct dvb_frontend_parameters *fep)
{
	struct dvb_ofdm_parameters *ofdm = &fep->u.ofdm;
	fe_code_rate_t *cr;
	u16 tps_val;
	int inv_test1,inv_test2;
	u32 dds_val, threshold = 0x800000;

	if (!rd(DIB3000MB_REG_TPS_LOCK))
		return 0;

	dds_val = ((rd(DIB3000MB_REG_DDS_VALUE_MSB) & 0xff) << 16) + rd(DIB3000MB_REG_DDS_VALUE_LSB);
	if (dds_val & threshold)
		inv_test1 = 0;
	else if (dds_val == threshold)
		inv_test1 = 1;
	else
		inv_test1 = 2;

	dds_val = ((rd(DIB3000MB_REG_DDS_FREQ_MSB) & 0xff) << 16) + rd(DIB3000MB_REG_DDS_FREQ_LSB);
	if (dds_val & threshold)
		inv_test2 = 0;
	else if (dds_val == threshold)
		inv_test2 = 1;
	else
		inv_test2 = 2;

	fep->inversion =
		((inv_test2 == 2) && (inv_test1==1 || inv_test1==0))
					||
		((inv_test2 == 0) && (inv_test1==1 || inv_test1==2));

	deb_getf("inversion %d %d, %d\n",inv_test2,inv_test1, fep->inversion);

	switch ((tps_val = rd(DIB3000MB_REG_TPS_QAM))) {
		case DIB3000MB_QAM_QPSK:
			deb_getf("QPSK ");
			ofdm->constellation = QPSK;
			break;
		case DIB3000MB_QAM_QAM16:
			deb_getf("QAM16 ");
			ofdm->constellation = QAM_16;
			break;
		case DIB3000MB_QAM_QAM64:
			deb_getf("QAM64 ");
			ofdm->constellation = QAM_64;
			break;
		default:
			err("Unexpected constellation returned by TPS (%d)",tps_val);
			break;
 	}
	deb_getf("TPS: %d\n",tps_val);

	if (rd(DIB3000MB_REG_TPS_HRCH)) {
		deb_getf("HRCH ON\n");
		tps_val = rd(DIB3000MB_REG_TPS_CODE_RATE_LP);
		cr = &ofdm->code_rate_LP;
		ofdm->code_rate_HP = FEC_NONE;

		switch ((tps_val = rd(DIB3000MB_REG_TPS_VIT_ALPHA))) {
			case DIB3000MB_VIT_ALPHA_OFF:
				deb_getf("HIERARCHY_NONE ");
				ofdm->hierarchy_information = HIERARCHY_NONE;
				break;
			case DIB3000MB_VIT_ALPHA_1:
				deb_getf("HIERARCHY_1 ");
				ofdm->hierarchy_information = HIERARCHY_1;
				break;
			case DIB3000MB_VIT_ALPHA_2:
				deb_getf("HIERARCHY_2 ");
				ofdm->hierarchy_information = HIERARCHY_2;
				break;
			case DIB3000MB_VIT_ALPHA_4:
				deb_getf("HIERARCHY_4 ");
				ofdm->hierarchy_information = HIERARCHY_4;
				break;
			default:
				err("Unexpected ALPHA value returned by TPS (%d)",tps_val);
		}
		deb_getf("TPS: %d\n",tps_val);
	} else {
		deb_getf("HRCH OFF\n");
		tps_val = rd(DIB3000MB_REG_TPS_CODE_RATE_HP);
		cr = &ofdm->code_rate_HP;
		ofdm->code_rate_LP = FEC_NONE;
		ofdm->hierarchy_information = HIERARCHY_NONE;
	}

	switch (tps_val) {
		case DIB3000MB_FEC_1_2:
			deb_getf("FEC_1_2 ");
			*cr = FEC_1_2;
			break;
		case DIB3000MB_FEC_2_3:
			deb_getf("FEC_2_3 ");
			*cr = FEC_2_3;
			break;
		case DIB3000MB_FEC_3_4:
			deb_getf("FEC_3_4 ");
			*cr = FEC_3_4;
			break;
		case DIB3000MB_FEC_5_6:
			deb_getf("FEC_5_6 ");
			*cr = FEC_4_5;
			break;
		case DIB3000MB_FEC_7_8:
			deb_getf("FEC_7_8 ");
			*cr = FEC_7_8;
			break;
		default:
			err("Unexpected FEC returned by TPS (%d)",tps_val);
			break;
	}
	deb_getf("TPS: %d\n",tps_val);

	switch ((tps_val = rd(DIB3000MB_REG_TPS_GUARD_TIME))) {
		case DIB3000MB_GUARD_TIME_1_32:
			deb_getf("GUARD_INTERVAL_1_32 ");
			ofdm->guard_interval = GUARD_INTERVAL_1_32;
			break;
		case DIB3000MB_GUARD_TIME_1_16:
			deb_getf("GUARD_INTERVAL_1_16 ");
			ofdm->guard_interval = GUARD_INTERVAL_1_16;
			break;
		case DIB3000MB_GUARD_TIME_1_8:
			deb_getf("GUARD_INTERVAL_1_8 ");
			ofdm->guard_interval = GUARD_INTERVAL_1_8;
			break;
		case DIB3000MB_GUARD_TIME_1_4:
			deb_getf("GUARD_INTERVAL_1_4 ");
			ofdm->guard_interval = GUARD_INTERVAL_1_4;
			break;
		default:
			err("Unexpected Guard Time returned by TPS (%d)",tps_val);
			break;
	}
	deb_getf("TPS: %d\n",tps_val);

	switch ((tps_val = rd(DIB3000MB_REG_TPS_FFT))) {
		case DIB3000MB_FFT_2K:
			deb_getf("TRANSMISSION_MODE_2K ");
			ofdm->transmission_mode = TRANSMISSION_MODE_2K;
			break;
		case DIB3000MB_FFT_8K:
			deb_getf("TRANSMISSION_MODE_8K ");
			ofdm->transmission_mode = TRANSMISSION_MODE_8K;
			break;
		default:
			err("unexpected transmission mode return by TPS (%d)",tps_val);
	}
	deb_getf("TPS: %d\n",tps_val);
	return 0;
}

static int dib3000mb_set_frontend(struct dib3000mb_state *state,
		struct dvb_frontend_parameters *fep, int tuner);

static int dib3000mb_fe_read_search_status(struct dib3000mb_state *state)
{
	u16 irq;
	struct dvb_frontend_parameters fep;

	irq = rd(DIB3000MB_REG_AS_IRQ_PENDING);

	if (irq & 0x02) {
		if (rd(DIB3000MB_REG_LOCK2_VALUE) & 0x01) {
			if (dib3000mb_get_frontend(state,&fep) == 0) {
				deb_setf("reading tuning data from frontend succeeded.\n");
				return dib3000mb_set_frontend(state,&fep,0) == 0;
			} else {
				deb_setf("reading tuning data failed -> tuning failed.\n");
				return 0;
			}
		} else {
			deb_setf("AS IRQ was pending, but LOCK2 was not & 0x01.\n");
			return 0;
		}
	} else if (irq & 0x01) {
		deb_setf("Autosearch failed.\n");
		return 0;
	}

	return -1;
}

static int dib3000mb_set_frontend(struct dib3000mb_state *state,
		struct dvb_frontend_parameters *fep, int tuner)
{
	struct dvb_ofdm_parameters *ofdm = &fep->u.ofdm;
	fe_code_rate_t fe_cr = FEC_NONE;
	int search_state,seq;

	if (tuner) {
		wr(DIB3000MB_REG_TUNER,
				DIB3000MB_ACTIVATE_TUNER_XFER( DIB3000MB_TUNER_ADDR_DEFAULT ) );
		dib3000mb_tuner_thomson_cable_eu(state,fep->frequency);

		/* wait for tuner */
		msleep(1);
		wr(DIB3000MB_REG_TUNER,
				DIB3000MB_DEACTIVATE_TUNER_XFER( DIB3000MB_TUNER_ADDR_DEFAULT ) );

		deb_setf("bandwidth: ");
		switch (ofdm->bandwidth) {
			case BANDWIDTH_8_MHZ:
				deb_setf("8 MHz\n");
				wr_foreach(dib3000mb_reg_timing_freq,dib3000mb_timing_freq[2]);
				wr_foreach(dib3000mb_reg_bandwidth,dib3000mb_bandwidth_8mhz);
				break;
			case BANDWIDTH_7_MHZ:
				deb_setf("7 MHz\n");
				wr_foreach(dib3000mb_reg_timing_freq,dib3000mb_timing_freq[1]);
				wr_foreach(dib3000mb_reg_bandwidth,dib3000mb_bandwidth_7mhz);
				break;
			case BANDWIDTH_6_MHZ:
				deb_setf("6 MHz\n");
				wr_foreach(dib3000mb_reg_timing_freq,dib3000mb_timing_freq[0]);
				wr_foreach(dib3000mb_reg_bandwidth,dib3000mb_bandwidth_6mhz);
				break;
			case BANDWIDTH_AUTO:
				return -EOPNOTSUPP;
			default:
				err("unkown bandwidth value.");
				return -EINVAL;
		}
	}
	wr(DIB3000MB_REG_LOCK1_MASK,DIB3000MB_LOCK1_SEARCH_4);

	deb_setf("transmission mode: ");
	switch (ofdm->transmission_mode) {
		case TRANSMISSION_MODE_2K:
			deb_setf("2k\n");
			wr(DIB3000MB_REG_FFT,DIB3000MB_FFT_2K);
			break;
		case TRANSMISSION_MODE_8K:
			deb_setf("8k\n");
			wr(DIB3000MB_REG_FFT,DIB3000MB_FFT_8K);
			break;
		case TRANSMISSION_MODE_AUTO:
			deb_setf("auto\n");
			wr(DIB3000MB_REG_FFT,DIB3000MB_FFT_AUTO);
			break;
		default:
			return -EINVAL;
	}

	deb_setf("guard: ");
	switch (ofdm->guard_interval) {
		case GUARD_INTERVAL_1_32:
			deb_setf("1_32\n");
			wr(DIB3000MB_REG_GUARD_TIME,DIB3000MB_GUARD_TIME_1_32);
			break;
		case GUARD_INTERVAL_1_16:
			deb_setf("1_16\n");
			wr(DIB3000MB_REG_GUARD_TIME,DIB3000MB_GUARD_TIME_1_16);
			break;
		case GUARD_INTERVAL_1_8:
			deb_setf("1_8\n");
			wr(DIB3000MB_REG_GUARD_TIME,DIB3000MB_GUARD_TIME_1_8);
			break;
		case GUARD_INTERVAL_1_4:
			deb_setf("1_4\n");
			wr(DIB3000MB_REG_GUARD_TIME,DIB3000MB_GUARD_TIME_1_4);
			break;
		case GUARD_INTERVAL_AUTO:
			deb_setf("auto\n");
			wr(DIB3000MB_REG_GUARD_TIME,DIB3000MB_GUARD_TIME_AUTO);
			break;
		default:
			return -EINVAL;
	}

	deb_setf("invsersion: ");
	switch (fep->inversion) {
		case INVERSION_AUTO:
			deb_setf("auto\n");
			break;
		case INVERSION_OFF:
			deb_setf("on\n");
			wr(DIB3000MB_REG_DDS_INV,DIB3000MB_DDS_INV_OFF);
			break;
		case INVERSION_ON:
			deb_setf("on\n");
			wr(DIB3000MB_REG_DDS_INV,DIB3000MB_DDS_INV_ON);
			break;
		default:
			return -EINVAL;
	}

	deb_setf("constellation: ");
	switch (ofdm->constellation) {
		case QPSK:
			deb_setf("qpsk\n");
			wr(DIB3000MB_REG_QAM,DIB3000MB_QAM_QPSK);
			break;
		case QAM_16:
			deb_setf("qam16\n");
			wr(DIB3000MB_REG_QAM,DIB3000MB_QAM_QAM16);
			break;
		case QAM_64:
			deb_setf("qam64\n");
			wr(DIB3000MB_REG_QAM,DIB3000MB_QAM_QAM64);
			break;
		case QAM_AUTO:
			break;
		default:
			return -EINVAL;
	}
	deb_setf("hierachy: ");	
	switch (ofdm->hierarchy_information) {
		case HIERARCHY_NONE:
			deb_setf("none ");
			/* fall through alpha is 1, even when HIERARCHY is NONE */ 
		case HIERARCHY_1:
			deb_setf("alpha=1\n");	
			wr(DIB3000MB_REG_VIT_ALPHA,DIB3000MB_VIT_ALPHA_1);
			break;
		case HIERARCHY_2:
			deb_setf("alpha=2\n");	
			wr(DIB3000MB_REG_VIT_ALPHA,DIB3000MB_VIT_ALPHA_2);
			break;
		case HIERARCHY_4:
			deb_setf("alpha=4\n");	
			wr(DIB3000MB_REG_VIT_ALPHA,DIB3000MB_VIT_ALPHA_4);
			break;
		case HIERARCHY_AUTO:
			deb_setf("alpha=auto\n");	
			wr(DIB3000MB_REG_VIT_ALPHA,DIB3000MB_VIT_ALPHA_AUTO);
			break;
		default:
			return -EINVAL;
	}

	deb_setf("hierarchy: ");
	if (ofdm->hierarchy_information == HIERARCHY_NONE) {
		deb_setf("none\n");
		wr(DIB3000MB_REG_VIT_HRCH,DIB3000MB_VIT_HRCH_OFF);
		wr(DIB3000MB_REG_VIT_HP,DIB3000MB_VIT_HP);
		fe_cr = ofdm->code_rate_HP;
	} else if (ofdm->hierarchy_information != HIERARCHY_AUTO) {
		deb_setf("on\n");
		wr(DIB3000MB_REG_VIT_HRCH,DIB3000MB_VIT_HRCH_ON);
		wr(DIB3000MB_REG_VIT_HP,DIB3000MB_VIT_LP);
		fe_cr = ofdm->code_rate_LP;
	}
	deb_setf("fec: ");
	switch (fe_cr) {
		case FEC_1_2:
			deb_setf("1_2\n");
			wr(DIB3000MB_REG_VIT_CODE_RATE,DIB3000MB_FEC_1_2);
			break;
		case FEC_2_3:
			deb_setf("2_3\n");
			wr(DIB3000MB_REG_VIT_CODE_RATE,DIB3000MB_FEC_2_3);
			break;
		case FEC_3_4:
			deb_setf("3_4\n");
			wr(DIB3000MB_REG_VIT_CODE_RATE,DIB3000MB_FEC_3_4);
			break;
		case FEC_5_6:
			deb_setf("5_6\n");
			wr(DIB3000MB_REG_VIT_CODE_RATE,DIB3000MB_FEC_5_6);
			break;
		case FEC_7_8:
			deb_setf("7_8\n");
			wr(DIB3000MB_REG_VIT_CODE_RATE,DIB3000MB_FEC_7_8);
			break;
		case FEC_NONE:
			deb_setf("none ");
		case FEC_AUTO:
			deb_setf("auto\n");
			break;
		default:
			return -EINVAL;
	}

	seq = dib3000mb_seq
		[ofdm->transmission_mode == TRANSMISSION_MODE_AUTO]
		[ofdm->guard_interval == GUARD_INTERVAL_AUTO]
		[fep->inversion == INVERSION_AUTO];

	deb_setf("seq? %d\n",seq);

	wr(DIB3000MB_REG_SEQ,seq);

	wr(DIB3000MB_REG_ISI,seq ? DIB3000MB_ISI_INHIBIT : DIB3000MB_ISI_ACTIVATE);

	if (ofdm->transmission_mode == TRANSMISSION_MODE_2K) {
		if (ofdm->guard_interval == GUARD_INTERVAL_1_8) {
			wr(DIB3000MB_REG_SYNC_IMPROVEMENT,DIB3000MB_SYNC_IMPROVE_2K_1_8);
		} else {
			wr(DIB3000MB_REG_SYNC_IMPROVEMENT,DIB3000MB_SYNC_IMPROVE_DEFAULT);
		}

		wr(DIB3000MB_REG_UNK_121,DIB3000MB_UNK_121_2K);
	} else {
		wr(DIB3000MB_REG_UNK_121,DIB3000MB_UNK_121_DEFAULT);
	}

	wr(DIB3000MB_REG_MOBILE_ALGO,DIB3000MB_MOBILE_ALGO_OFF);
	wr(DIB3000MB_REG_MOBILE_MODE_QAM,DIB3000MB_MOBILE_MODE_QAM_OFF);
	wr(DIB3000MB_REG_MOBILE_MODE,DIB3000MB_MOBILE_MODE_OFF);

	wr_foreach(dib3000mb_reg_agc_bandwidth,dib3000mb_agc_bandwidth_high);

	wr(DIB3000MB_REG_ISI,DIB3000MB_ISI_ACTIVATE);

	wr(DIB3000MB_REG_RESTART,DIB3000MB_RESTART_AGC+DIB3000MB_RESTART_CTRL);
	wr(DIB3000MB_REG_RESTART,DIB3000MB_RESTART_OFF);

	/* wait for AGC lock */
	msleep(70);

	wr_foreach(dib3000mb_reg_agc_bandwidth,dib3000mb_agc_bandwidth_low);

	/* something has to be auto searched */
	if (ofdm->constellation == QAM_AUTO ||
		ofdm->hierarchy_information == HIERARCHY_AUTO ||
		fe_cr == FEC_AUTO ||
		fep->inversion == INVERSION_AUTO) {

		deb_setf("autosearch enabled.\n");	

		wr(DIB3000MB_REG_ISI,DIB3000MB_ISI_INHIBIT);

		wr(DIB3000MB_REG_RESTART,DIB3000MB_RESTART_AUTO_SEARCH);
		wr(DIB3000MB_REG_RESTART,DIB3000MB_RESTART_OFF);

		while ((search_state = dib3000mb_fe_read_search_status(state)) < 0);
		deb_info("search_state after autosearch %d\n",search_state);
		return search_state ? 0 : -EINVAL;
	} else {
		wr(DIB3000MB_REG_RESTART,DIB3000MB_RESTART_CTRL);
		wr(DIB3000MB_REG_RESTART,DIB3000MB_RESTART_OFF);
		msleep(70);
	}
	return 0;
}


static int dib3000mb_fe_init(struct dib3000mb_state *state,int mobile_mode)
{
	wr(DIB3000MB_REG_POWER_CONTROL,DIB3000MB_POWER_UP);

	wr(DIB3000MB_REG_RESTART, DIB3000MB_RESTART_AGC);

	wr(DIB3000MB_REG_RESET_DEVICE,DIB3000MB_RESET_DEVICE);
	wr(DIB3000MB_REG_RESET_DEVICE,DIB3000MB_RESET_DEVICE_RST);

	wr(DIB3000MB_REG_CLOCK,DIB3000MB_CLOCK_DEFAULT);

	wr(DIB3000MB_REG_ELECT_OUT_MODE,DIB3000MB_ELECT_OUT_MODE_ON);

	wr(DIB3000MB_REG_QAM,DIB3000MB_QAM_RESERVED);
	wr(DIB3000MB_REG_VIT_ALPHA,DIB3000MB_VIT_ALPHA_AUTO);

	wr(DIB3000MB_REG_DDS_FREQ_MSB,DIB3000MB_DDS_FREQ_MSB);
	wr(DIB3000MB_REG_DDS_FREQ_LSB,DIB3000MB_DDS_FREQ_LSB);

	wr_foreach(dib3000mb_reg_timing_freq,dib3000mb_timing_freq[2]);

	wr_foreach(dib3000mb_reg_impulse_noise,
			dib3000mb_impulse_noise_values[DIB3000MB_IMPNOISE_OFF]);

	wr_foreach(dib3000mb_reg_agc_gain,dib3000mb_default_agc_gain);

	wr(DIB3000MB_REG_PHASE_NOISE,DIB3000MB_PHASE_NOISE_DEFAULT);

	wr_foreach(dib3000mb_reg_phase_noise, dib3000mb_default_noise_phase);

	wr_foreach(dib3000mb_reg_lock_duration,dib3000mb_default_lock_duration);

	wr_foreach(dib3000mb_reg_agc_bandwidth,dib3000mb_agc_bandwidth_low);

	wr(DIB3000MB_REG_LOCK0_MASK,DIB3000MB_LOCK0_DEFAULT);
	wr(DIB3000MB_REG_LOCK1_MASK,DIB3000MB_LOCK1_SEARCH_4);
	wr(DIB3000MB_REG_LOCK2_MASK,DIB3000MB_LOCK2_DEFAULT);
	wr(DIB3000MB_REG_SEQ,dib3000mb_seq[1][1][1]);

	wr_foreach(dib3000mb_reg_bandwidth,dib3000mb_bandwidth_8mhz);

	wr(DIB3000MB_REG_UNK_68,DIB3000MB_UNK_68);
	wr(DIB3000MB_REG_UNK_69,DIB3000MB_UNK_69);
	wr(DIB3000MB_REG_UNK_71,DIB3000MB_UNK_71);
	wr(DIB3000MB_REG_UNK_77,DIB3000MB_UNK_77);
	wr(DIB3000MB_REG_UNK_78,DIB3000MB_UNK_78);
	wr(DIB3000MB_REG_ISI,DIB3000MB_ISI_INHIBIT);
	wr(DIB3000MB_REG_UNK_92,DIB3000MB_UNK_92);
	wr(DIB3000MB_REG_UNK_96,DIB3000MB_UNK_96);
	wr(DIB3000MB_REG_UNK_97,DIB3000MB_UNK_97);
	wr(DIB3000MB_REG_UNK_106,DIB3000MB_UNK_106);
	wr(DIB3000MB_REG_UNK_107,DIB3000MB_UNK_107);
	wr(DIB3000MB_REG_UNK_108,DIB3000MB_UNK_108);
	wr(DIB3000MB_REG_UNK_122,DIB3000MB_UNK_122);
	wr(DIB3000MB_REG_MOBILE_MODE_QAM,DIB3000MB_MOBILE_MODE_QAM_OFF);
	wr(DIB3000MB_REG_VIT_CODE_RATE,DIB3000MB_FEC_1_2);
	wr(DIB3000MB_REG_VIT_HP,DIB3000MB_VIT_HP);
	wr(DIB3000MB_REG_BERLEN,DIB3000MB_BERLEN_DEFAULT);

	wr_foreach(dib3000mb_reg_filter_coeffs,dib3000mb_filter_coeffs);

	wr(DIB3000MB_REG_MOBILE_ALGO,DIB3000MB_MOBILE_ALGO_ON);
	wr(DIB3000MB_REG_MULTI_DEMOD_MSB,DIB3000MB_MULTI_DEMOD_MSB);
	wr(DIB3000MB_REG_MULTI_DEMOD_LSB,DIB3000MB_MULTI_DEMOD_LSB);

	wr(DIB3000MB_REG_OUTPUT_MODE,DIB3000MB_OUTPUT_MODE_SLAVE);

	wr(DIB3000MB_REG_FIFO_142,DIB3000MB_FIFO_142);
	wr(DIB3000MB_REG_MPEG2_OUT_MODE,DIB3000MB_MPEG2_OUT_MODE_188);
	wr(DIB3000MB_REG_FIFO_144,DIB3000MB_FIFO_144);
	wr(DIB3000MB_REG_FIFO,DIB3000MB_FIFO_INHIBIT);
	wr(DIB3000MB_REG_FIFO_146,DIB3000MB_FIFO_146);
	wr(DIB3000MB_REG_FIFO_147,DIB3000MB_FIFO_147);

	wr(DIB3000MB_REG_DATA_IN_DIVERSITY,DIB3000MB_DATA_DIVERSITY_IN_OFF);
	return 0;
}

static int dib3000mb_read_status(struct dib3000mb_state *state,fe_status_t *stat)
{
	*stat = 0;

	if (rd(DIB3000MB_REG_AGC_LOCK))
		*stat |= FE_HAS_SIGNAL;
	if (rd(DIB3000MB_REG_CARRIER_LOCK))
		*stat |= FE_HAS_CARRIER;
	if (rd(DIB3000MB_REG_VIT_LCK))
		*stat |= FE_HAS_VITERBI;
	if (rd(DIB3000MB_REG_TS_SYNC_LOCK))
		*stat |= (FE_HAS_SYNC | FE_HAS_LOCK);

	deb_info("actual status is %2x\n",*stat);

	deb_getf("tps %x %x %x %x %x\n",
			rd(DIB3000MB_REG_TPS_1),
			rd(DIB3000MB_REG_TPS_2),
			rd(DIB3000MB_REG_TPS_3),
			rd(DIB3000MB_REG_TPS_4),
			rd(DIB3000MB_REG_TPS_5));
	
	deb_info("autoval: tps: %d, qam: %d, hrch: %d, alpha: %d, hp: %d, lp: %d, guard: %d, fft: %d cell: %d\n",
			rd(DIB3000MB_REG_TPS_LOCK),
			rd(DIB3000MB_REG_TPS_QAM),
			rd(DIB3000MB_REG_TPS_HRCH),
			rd(DIB3000MB_REG_TPS_VIT_ALPHA),
			rd(DIB3000MB_REG_TPS_CODE_RATE_HP),
			rd(DIB3000MB_REG_TPS_CODE_RATE_LP),
			rd(DIB3000MB_REG_TPS_GUARD_TIME),
			rd(DIB3000MB_REG_TPS_FFT),
			rd(DIB3000MB_REG_TPS_CELL_ID));

	//*stat = FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC | FE_HAS_LOCK;
	return 0;
}

static int dib3000mb_read_ber(struct dib3000mb_state *state,u32 *ber)
{
	*ber = ((rd(DIB3000MB_REG_BER_MSB) << 16) | rd(DIB3000MB_REG_BER_LSB) );
	return 0;
}
/*
 * Amaury:
 * signal strength is measured with dBm (power compared to mW)
 * the standard range is -90dBm(low power) to -10 dBm (strong power),
 * but the calibration is done for -100 dBm to 0dBm
 */

#define DIB3000MB_AGC_REF_dBm		-14
#define DIB3000MB_GAIN_SLOPE_dBm	100
#define DIB3000MB_GAIN_DELTA_dBm	-2
static int dib3000mb_read_signal_strength(struct dib3000mb_state *state, u16 *strength)
{
/* TODO log10 
	u16 sigpow = rd(DIB3000MB_REG_SIGNAL_POWER), 
		n_agc_power = rd(DIB3000MB_REG_AGC_POWER),
		rf_power = rd(DIB3000MB_REG_RF_POWER);
	double rf_power_dBm, ad_power_dBm, minar_power_dBm;
	
	if (n_agc_power == 0 )
		n_agc_power = 1 ;

	ad_power_dBm    = 10 * log10 ( (float)n_agc_power / (float)(1<<16) );
	minor_power_dBm = ad_power_dBm - DIB3000MB_AGC_REF_dBm;
	rf_power_dBm = (-DIB3000MB_GAIN_SLOPE_dBm * (float)rf_power / (float)(1<<16) + 
			DIB3000MB_GAIN_DELTA_dBm) + minor_power_dBm;
	// relative rf_power 
	*strength = (u16) ((rf_power_dBm + 100) / 100 * 0xffff);
*/
	*strength = rd(DIB3000MB_REG_SIGNAL_POWER) * 0xffff / 0x170;
	return 0;
}

/*
 * Amaury: 
 * snr is the signal quality measured in dB.
 * snr = 10*log10(signal power / noise power)
 * the best quality is near 35dB (cable transmission & good modulator)
 * the minimum without errors depend of transmission parameters
 * some indicative values are given in en300744 Annex A
 * ex : 16QAM 2/3 (Gaussian)  = 11.1 dB
 *
 * If SNR is above 20dB, BER should be always 0.
 * choose 0dB as the minimum
 */
static int dib3000mb_read_snr(struct dib3000mb_state *state,u16 *snr)
{
	short sigpow = rd(DIB3000MB_REG_SIGNAL_POWER);
	int icipow = ((rd(DIB3000MB_REG_NOISE_POWER_MSB) & 0xff) << 16) |
		rd(DIB3000MB_REG_NOISE_POWER_LSB);
/*
	float snr_dBm=0;

	if (sigpow > 0 && icipow > 0)
		snr_dBm = 10.0 * log10( (float) (sigpow<<8) / (float)icipow )  ;
	else if (sigpow > 0)
		snr_dBm = 35;
	
	*snr = (u16) ((snr_dBm / 35) * 0xffff);
*/
	*snr = (sigpow<<8) / (icipow > 0 ? icipow : 1);
	return 0;
}

static int dib3000mb_read_unc_blocks(struct dib3000mb_state *state,u32 *unc)
{
	*unc = rd(DIB3000MB_REG_UNC);
	return 0;
}

static int dib3000mb_sleep(struct dib3000mb_state *state)
{
	wr(DIB3000MB_REG_POWER_CONTROL,DIB3000MB_POWER_DOWN);
	return 0;
}

static int dib3000mb_fe_get_tune_settings(struct dib3000mb_state *state, 
		struct dvb_frontend_tune_settings *tune)
{
	tune->min_delay_ms = 800;
	tune->step_size = 166667;
	tune->max_drift = 166667*2;
					
	return 0;
}

static int dib3000mb_ioctl (struct dvb_frontend *fe, unsigned int cmd, void *arg)
{
	struct dib3000mb_state *state = fe->data;
	switch (cmd) {
		case FE_GET_INFO:
			deb_info("FE_GET_INFO\n");
			memcpy(arg, &dib3000mb_info, sizeof(struct dvb_frontend_info));
			return 0;
			break;

		case FE_READ_STATUS:
			deb_info("FE_READ_STATUS\n");
			return dib3000mb_read_status(state,(fe_status_t *)arg);
			break;

		case FE_READ_BER:
			deb_info("FE_READ_BER\n");
			return dib3000mb_read_ber(state,(u32 *)arg);
			break;

		case FE_READ_SIGNAL_STRENGTH:
			deb_info("FE_READ_SIG_STRENGTH\n");
			return dib3000mb_read_signal_strength(state,(u16 *) arg);
			break;

		case FE_READ_SNR:
			deb_info("FE_READ_SNR\n");
			return dib3000mb_read_snr(state,(u16 *) arg);
			break;

		case FE_READ_UNCORRECTED_BLOCKS:
			deb_info("FE_READ_UNCORRECTED_BLOCKS\n");
			return dib3000mb_read_unc_blocks(state,(u32 *) arg);
			break;

		case FE_SET_FRONTEND:
			deb_info("FE_SET_FRONTEND\n");
			return dib3000mb_set_frontend(state,(struct dvb_frontend_parameters *) arg,1);
			break;

		case FE_GET_FRONTEND:
			deb_info("FE_GET_FRONTEND\n");
			return dib3000mb_get_frontend(state,(struct dvb_frontend_parameters *) arg);
			break;

		case FE_SLEEP:
			deb_info("FE_SLEEP\n");
			return dib3000mb_sleep(state);
			break;

		case FE_INIT:
			deb_info("FE_INIT\n");
			return dib3000mb_fe_init(state,0);
			break;

		case FE_GET_TUNE_SETTINGS:
			deb_info("GET_TUNE_SETTINGS");
			return dib3000mb_fe_get_tune_settings(state, (struct
						dvb_frontend_tune_settings *) arg);

			break;
		case FE_SET_TONE:
		case FE_SET_VOLTAGE:
		default:
			return -EOPNOTSUPP;
			break;
	}
	return 0;
}

static struct i2c_client client_template;

static int dib3000mb_attach_adapter(struct i2c_adapter *adapter)
{
	struct i2c_client *client;
	struct dib3000mb_state *state;
	int ret = -ENOMEM;

	deb_info("i2c probe with adapter '%s'.\n",adapter->name);

	if ((state = kmalloc(sizeof(struct dib3000mb_state),GFP_KERNEL)) == NULL)
		return -ENOMEM;


	if ((client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL)) == NULL)
		goto i2c_kmalloc_err;

	memcpy(client, &client_template, sizeof(struct i2c_client));

	client->adapter = adapter;
	client->addr = 0x10;
	state->i2c = client;

	i2c_set_clientdata(client,state);

	state->manufactor_id = dib3000mb_read_reg(client, DIB3000MB_REG_MANUFACTOR_ID);
	if (state->manufactor_id != 0x01b3) {
		ret = -ENODEV;
		goto probe_err;
	}
	
	state->device_id = dib3000mb_read_reg(client,DIB3000MB_REG_DEVICE_ID);
	if (state->device_id != 0x3000) {
		ret = -ENODEV;
		goto probe_err;
	}

	info("found a DiBCom (0x%04x) 3000-MB DVB-T frontend (ver: %x).",
			state->manufactor_id, state->device_id);
	
	if ((ret = i2c_attach_client(client)))
		goto i2c_attach_err;

	if (state->dvb == NULL)
		goto i2c_attach_err;

	if ((ret = dvb_register_frontend(dib3000mb_ioctl, state->dvb, state,
					     &dib3000mb_info, THIS_MODULE)))
		goto dvb_fe_err;


	goto success;
dvb_fe_err:
	i2c_detach_client(client);
i2c_attach_err:
probe_err:
	kfree(client);
i2c_kmalloc_err:
	kfree(state);
	return ret;
success:
	return 0;
}


static int dib3000mb_detach_client(struct i2c_client *client)
{
	struct dib3000mb_state *state = i2c_get_clientdata(client);

	deb_info("i2c detach\n");

	dvb_unregister_frontend(dib3000mb_ioctl, state->dvb);
	i2c_detach_client(client);
	kfree(client);
	kfree(state);

	return 0;
}

static int dib3000mb_command(struct i2c_client *client,
			      unsigned int cmd, void *arg)
{
	struct dib3000mb_state *state = i2c_get_clientdata(client);
	deb_info("i2c command.\n");
	switch(cmd) {
		case FE_REGISTER:
			state->dvb = arg;
			break;
		case FE_UNREGISTER:
			state->dvb = NULL;
			break;
		default:
			return -EOPNOTSUPP;
	}

	return 0;
}

static struct i2c_driver driver = {
	.owner		= THIS_MODULE,
	.name		= "dib3000mb",
	.id			= I2C_DRIVERID_DVBFE_DIB3000MB,
	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= dib3000mb_attach_adapter,
	.detach_client	= dib3000mb_detach_client,
	.command	= dib3000mb_command,
};

static struct i2c_client client_template = {
	.name		= "dib3000mb",
	.flags		= I2C_CLIENT_ALLOW_USE,
	.driver		= &driver,
};

/* module stuff */
static int __init dib3000mb_init(void)
{
	deb_info("debugging level: %d\n",debug);
	return i2c_add_driver(&driver);
}

static void __exit dib3000mb_exit(void)
{
	i2c_del_driver(&driver);
}

module_init (dib3000mb_init);
module_exit (dib3000mb_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
