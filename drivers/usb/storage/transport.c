/* Driver for USB Mass Storage compliant devices
 *
 * $Id: transport.c,v 1.47 2002/04/22 03:39:43 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Developed with the assistance of:
 *   (c) 2000 David L. Brown, Jr. (usb-storage@davidb.org)
 *   (c) 2000 Stephen J. Gowdy (SGowdy@lbl.gov)
 *   (c) 2002 Alan Stern <stern@rowland.org>
 *
 * Initial work by:
 *   (c) 1999 Michael Gee (michael@linuxspecific.com)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include "transport.h"
#include "protocol.h"
#include "usb.h"
#include "debug.h"

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/slab.h>

/***********************************************************************
 * Data transfer routines
 ***********************************************************************/

/*
 * This is subtle, so pay attention:
 * ---------------------------------
 * We're very concerned about races with a command abort.  Hanging this code
 * is a sure fire way to hang the kernel.  (Note that this discussion applies
 * only to transactions resulting from a scsi queued-command, since only
 * these transactions are subject to a scsi abort.  Other transactions, such
 * as those occurring during device-specific initialization, must be handled
 * by a separate code path.)
 *
 * The abort function (usb_storage_command_abort() in scsiglue.c) first
 * sets the machine state and the ABORTING bit in us->flags to prevent
 * new URBs from being submitted.  It then calls usb_stor_stop_transport()
 * below, which atomically tests-and-clears the URB_ACTIVE bit in us->flags
 * to see if the current_urb needs to be stopped.  Likewise, the SG_ACTIVE
 * bit is tested to see if the current_sg scatter-gather request needs to be
 * stopped.  The timeout callback routine does much the same thing.
 *
 * When a disconnect occurs, the DISCONNECTING bit in us->flags is set to
 * prevent new URBs from being submitted, and usb_stor_stop_transport() is
 * called to stop any ongoing requests.
 *
 * The submit function first verifies that the submitting is allowed
 * (neither ABORTING nor DISCONNECTING bits are set) and that the submit
 * completes without errors, and only then sets the URB_ACTIVE bit.  This
 * prevents the stop_transport() function from trying to cancel the URB
 * while the submit call is underway.  Next, the submit function must test
 * the flags to see if an abort or disconnect occurred during the submission
 * or before the URB_ACTIVE bit was set.  If so, it's essential to cancel
 * the URB if it hasn't been cancelled already (i.e., if the URB_ACTIVE bit
 * is still set).  Either way, the function must then wait for the URB to
 * finish.  Note that because the URB_ASYNC_UNLINK flag is set, the URB can
 * still be in progress even after a call to usb_unlink_urb() returns.
 *
 * The idea is that (1) once the ABORTING or DISCONNECTING bit is set,
 * either the stop_transport() function or the submitting function
 * is guaranteed to call usb_unlink_urb() for an active URB,
 * and (2) test_and_clear_bit() prevents usb_unlink_urb() from being
 * called more than once or from being called during usb_submit_urb().
 */

/* This is the completion handler which will wake us up when an URB
 * completes.
 */
static void usb_stor_blocking_completion(struct urb *urb, struct pt_regs *regs)
{
	struct completion *urb_done_ptr = (struct completion *)urb->context;

	complete(urb_done_ptr);
}
 
/* This is the timeout handler which will cancel an URB when its timeout
 * expires.
 */
static void timeout_handler(unsigned long us_)
{
	struct us_data *us = (struct us_data *) us_;

	if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->flags)) {
		US_DEBUGP("Timeout -- cancelling URB\n");
		usb_unlink_urb(us->current_urb);
	}
}

/* This is the common part of the URB message submission code
 *
 * All URBs from the usb-storage driver involved in handling a queued scsi
 * command _must_ pass through this function (or something like it) for the
 * abort mechanisms to work properly.
 */
static int usb_stor_msg_common(struct us_data *us, int timeout)
{
	struct completion urb_done;
	struct timer_list to_timer;
	int status;

	/* don't submit URBS during abort/disconnect processing */
	if (us->flags & DONT_SUBMIT)
		return -ECONNRESET;

	/* set up data structures for the wakeup system */
	init_completion(&urb_done);

	/* fill the common fields in the URB */
	us->current_urb->context = &urb_done;
	us->current_urb->actual_length = 0;
	us->current_urb->error_count = 0;
	us->current_urb->transfer_flags = URB_ASYNC_UNLINK;
	us->current_urb->status = 0;

	/* submit the URB */
	status = usb_submit_urb(us->current_urb, GFP_NOIO);
	if (status) {
		/* something went wrong */
		return status;
	}

	/* since the URB has been submitted successfully, it's now okay
	 * to cancel it */
	set_bit(US_FLIDX_URB_ACTIVE, &us->flags);

	/* did an abort/disconnect occur during the submission? */
	if (us->flags & DONT_SUBMIT) {

		/* cancel the URB, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->flags)) {
			US_DEBUGP("-- cancelling URB\n");
			usb_unlink_urb(us->current_urb);
		}
	}
 
	/* submit the timeout timer, if a timeout was requested */
	if (timeout > 0) {
		init_timer(&to_timer);
		to_timer.expires = jiffies + timeout;
		to_timer.function = timeout_handler;
		to_timer.data = (unsigned long) us;
		add_timer(&to_timer);
	}

	/* wait for the completion of the URB */
	wait_for_completion(&urb_done);
	clear_bit(US_FLIDX_URB_ACTIVE, &us->flags);
 
	/* clean up the timeout timer */
	if (timeout > 0)
		del_timer_sync(&to_timer);

	/* return the URB status */
	return us->current_urb->status;
}

/*
 * Transfer one control message, with timeouts, and allowing early
 * termination.  Return codes are usual -Exxx, *not* USB_STOR_XFER_xxx.
 */
int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		 u8 request, u8 requesttype, u16 value, u16 index, 
		 void *data, u16 size, int timeout)
{
	int status;

	US_DEBUGP("%s: rq=%02x rqtype=%02x value=%04x index=%02x len=%u\n",
			__FUNCTION__, request, requesttype,
			value, index, size);

	/* fill in the devrequest structure */
	us->dr->bRequestType = requesttype;
	us->dr->bRequest = request;
	us->dr->wValue = cpu_to_le16(value);
	us->dr->wIndex = cpu_to_le16(index);
	us->dr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->dr, data, size, 
			 usb_stor_blocking_completion, NULL);
	status = usb_stor_msg_common(us, timeout);

	/* return the actual length of the data transferred if no error */
	if (status == 0)
		status = us->current_urb->actual_length;
	return status;
}

/* This is a version of usb_clear_halt() that allows early termination and
 * doesn't read the status from the device -- this is because some devices
 * crash their internal firmware when the status is requested after a halt.
 *
 * A definitive list of these 'bad' devices is too difficult to maintain or
 * make complete enough to be useful.  This problem was first observed on the
 * Hagiwara FlashGate DUAL unit.  However, bus traces reveal that neither
 * MacOS nor Windows checks the status after clearing a halt.
 *
 * Since many vendors in this space limit their testing to interoperability
 * with these two OSes, specification violations like this one are common.
 */
int usb_stor_clear_halt(struct us_data *us, unsigned int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe);

	if (usb_pipein (pipe))
		endp |= USB_DIR_IN;

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT, 0,
		endp, NULL, 0, 3*HZ);

	/* reset the toggles and endpoint flags */
	usb_endpoint_running(us->pusb_dev, usb_pipeendpoint(pipe),
		usb_pipeout(pipe));
	usb_settoggle(us->pusb_dev, usb_pipeendpoint(pipe),
		usb_pipeout(pipe), 0);

	US_DEBUGP("%s: result = %d\n", __FUNCTION__, result);
	return result;
}


/*
 * Interpret the results of a URB transfer
 *
 * This function prints appropriate debugging messages, clears halts on
 * non-control endpoints, and translates the status to the corresponding
 * USB_STOR_XFER_xxx return code.
 */
static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial)
{
	US_DEBUGP("Status code %d; transferred %u/%u\n",
			result, partial, length);
	switch (result) {

	/* no error code; did we send all the data? */
	case 0:
		if (partial != length) {
			US_DEBUGP("-- short transfer\n");
			return USB_STOR_XFER_SHORT;
		}

		US_DEBUGP("-- transfer complete\n");
		return USB_STOR_XFER_GOOD;

	/* stalled */
	case -EPIPE:
		/* for control endpoints, (used by CB[I]) a stall indicates
		 * a failed command */
		if (usb_pipecontrol(pipe)) {
			US_DEBUGP("-- stall on control pipe\n");
			return USB_STOR_XFER_STALLED;
		}

		/* for other sorts of endpoint, clear the stall */
		US_DEBUGP("clearing endpoint halt for pipe 0x%x\n", pipe);
		if (usb_stor_clear_halt(us, pipe) < 0)
			return USB_STOR_XFER_ERROR;
		return USB_STOR_XFER_STALLED;

	/* NAK - that means we've retried this a few times already */
	case -ETIMEDOUT:
		US_DEBUGP("-- device NAKed\n");
		return USB_STOR_XFER_ERROR;

	/* the transfer was cancelled, presumably by an abort */
	case -ECONNRESET:
		US_DEBUGP("-- transfer cancelled\n");
		return USB_STOR_XFER_ERROR;

	/* short scatter-gather read transfer */
	case -EREMOTEIO:
		US_DEBUGP("-- short read transfer\n");
		return USB_STOR_XFER_SHORT;

	/* the catch-all error case */
	default:
		US_DEBUGP("-- unknown error\n");
		return USB_STOR_XFER_ERROR;
	}
}

/*
 * Transfer one control message, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.
 */
int usb_stor_ctrl_transfer(struct us_data *us, unsigned int pipe,
		u8 request, u8 requesttype, u16 value, u16 index,
		void *data, u16 size)
{
	int result;

	US_DEBUGP("%s: rq=%02x rqtype=%02x value=%04x index=%02x len=%u\n",
			__FUNCTION__, request, requesttype,
			value, index, size);

	/* fill in the devrequest structure */
	us->dr->bRequestType = requesttype;
	us->dr->bRequest = request;
	us->dr->wValue = cpu_to_le16(value);
	us->dr->wIndex = cpu_to_le16(index);
	us->dr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->dr, data, size, 
			 usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	return interpret_urb_result(us, pipe, size, result,
			us->current_urb->actual_length);
}

/*
 * Receive one interrupt buffer, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.
 *
 * This routine always uses us->recv_intr_pipe as the pipe and
 * us->ep_bInterval as the interrupt interval.
 */
int usb_stor_intr_transfer(struct us_data *us, void *buf, unsigned int length)
{
	int result;
	unsigned int pipe = us->recv_intr_pipe;
	unsigned int maxp;

	US_DEBUGP("%s: xfer %u bytes\n", __FUNCTION__, length);

	/* calculate the max packet size */
	maxp = usb_maxpacket(us->pusb_dev, pipe, usb_pipeout(pipe));
	if (maxp > length)
		maxp = length;

	/* fill and submit the URB */
	usb_fill_int_urb(us->current_urb, us->pusb_dev, pipe, buf,
			maxp, usb_stor_blocking_completion, NULL,
			us->ep_bInterval);
	result = usb_stor_msg_common(us, 0);

	return interpret_urb_result(us, pipe, length, result,
			us->current_urb->actual_length);
}

/*
 * Transfer one buffer via bulk pipe, without timeouts, but allowing early
 * termination.  Return codes are USB_STOR_XFER_xxx.  If the bulk pipe
 * stalls during the transfer, the halt is automatically cleared.
 */
int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len)
{
	int result;

	US_DEBUGP("%s: xfer %u bytes\n", __FUNCTION__, length);

	/* fill and submit the URB */
	usb_fill_bulk_urb(us->current_urb, us->pusb_dev, pipe, buf, length,
		      usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	/* store the actual length of the data transferred */
	if (act_len)
		*act_len = us->current_urb->actual_length;
	return interpret_urb_result(us, pipe, length, result, 
			us->current_urb->actual_length);
}

/*
 * Transfer a scatter-gather list via bulk transfer
 *
 * This function does basically the same thing as usb_stor_bulk_transfer_buf()
 * above, but it uses the usbcore scatter-gather library.
 */
int usb_stor_bulk_transfer_sglist(struct us_data *us, unsigned int pipe,
		struct scatterlist *sg, int num_sg, unsigned int length,
		unsigned int *act_len)
{
	int result;

	/* don't submit s-g requests during abort/disconnect processing */
	if (us->flags & DONT_SUBMIT)
		return USB_STOR_XFER_ERROR;

	/* initialize the scatter-gather request block */
	US_DEBUGP("%s: xfer %u bytes, %d entries\n", __FUNCTION__,
			length, num_sg);
	result = usb_sg_init(us->current_sg, us->pusb_dev, pipe, 0,
			sg, num_sg, length, SLAB_NOIO);
	if (result) {
		US_DEBUGP("usb_sg_init returned %d\n", result);
		return USB_STOR_XFER_ERROR;
	}

	/* since the block has been initialized successfully, it's now
	 * okay to cancel it */
	set_bit(US_FLIDX_SG_ACTIVE, &us->flags);

	/* did an abort/disconnect occur during the submission? */
	if (us->flags & DONT_SUBMIT) {

		/* cancel the request, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->flags)) {
			US_DEBUGP("-- cancelling sg request\n");
			usb_sg_cancel(us->current_sg);
		}
	}

	/* wait for the completion of the transfer */
	usb_sg_wait(us->current_sg);
	clear_bit(US_FLIDX_SG_ACTIVE, &us->flags);

	result = us->current_sg->status;
	if (act_len)
		*act_len = us->current_sg->bytes;
	return interpret_urb_result(us, pipe, length, result,
			us->current_sg->bytes);
}

/*
 * Transfer an entire SCSI command's worth of data payload over the bulk
 * pipe.
 *
 * Note that this uses usb_stor_bulk_transfer_buf() and
 * usb_stor_bulk_transfer_sglist() to achieve its goals --
 * this function simply determines whether we're going to use
 * scatter-gather or not, and acts appropriately.
 */
int usb_stor_bulk_transfer_sg(struct us_data* us, unsigned int pipe,
		void *buf, unsigned int length_left, int use_sg, int *residual)
{
	int result;
	unsigned int partial;

	/* are we scatter-gathering? */
	if (use_sg) {
		/* use the usb core scatter-gather primitives */
		result = usb_stor_bulk_transfer_sglist(us, pipe,
				(struct scatterlist *) buf, use_sg,
				length_left, &partial);
		length_left -= partial;
	} else {
		/* no scatter-gather, just make the request */
		result = usb_stor_bulk_transfer_buf(us, pipe, buf, 
				length_left, &partial);
		length_left -= partial;
	}

	/* store the residual and return the error code */
	if (residual)
		*residual = length_left;
	return result;
}

/***********************************************************************
 * Transport routines
 ***********************************************************************/

/* Invoke the transport and basic error-handling/recovery methods
 *
 * This is used by the protocol layers to actually send the message to
 * the device and receive the response.
 */
void usb_stor_invoke_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	int need_auto_sense;
	int result;

	/* send the command to the transport layer */
	srb->resid = 0;
	result = us->transport(srb, us);

	/* if the command gets aborted by the higher layers, we need to
	 * short-circuit all other processing
	 */
	if (atomic_read(&us->sm_state) == US_STATE_ABORTING) {
		US_DEBUGP("-- command was aborted\n");
		goto Handle_Abort;
	}

	/* if there is a transport error, reset and don't auto-sense */
	if (result == USB_STOR_TRANSPORT_ERROR) {
		US_DEBUGP("-- transport indicates error, resetting\n");
		us->transport_reset(us);
		srb->result = DID_ERROR << 16;
		return;
	}

	/* Determine if we need to auto-sense
	 *
	 * I normally don't use a flag like this, but it's almost impossible
	 * to understand what's going on here if I don't.
	 */
	need_auto_sense = 0;

	/*
	 * If we're running the CB transport, which is incapable
	 * of determining status on it's own, we need to auto-sense almost
	 * every time.
	 */
	if (us->protocol == US_PR_CB || us->protocol == US_PR_DPCM_USB) {
		US_DEBUGP("-- CB transport device requiring auto-sense\n");
		need_auto_sense = 1;

		/* There are some exceptions to this.  Notably, if this is
		 * a UFI device and the command is REQUEST_SENSE or INQUIRY,
		 * then it is impossible to truly determine status.
		 */
		if (us->subclass == US_SC_UFI &&
		    ((srb->cmnd[0] == REQUEST_SENSE) ||
		     (srb->cmnd[0] == INQUIRY))) {
			US_DEBUGP("** no auto-sense for a special command\n");
			need_auto_sense = 0;
		}
	}

	/*
	 * If we have a failure, we're going to do a REQUEST_SENSE 
	 * automatically.  Note that we differentiate between a command
	 * "failure" and an "error" in the transport mechanism.
	 */
	if (result == USB_STOR_TRANSPORT_FAILED) {
		US_DEBUGP("-- transport indicates command failure\n");
		need_auto_sense = 1;
	}

	/*
	 * Also, if we have a short transfer on a command that can't have
	 * a short transfer, we're going to do this.
	 */
	if ((srb->resid > 0) &&
	    !((srb->cmnd[0] == REQUEST_SENSE) ||
	      (srb->cmnd[0] == INQUIRY) ||
	      (srb->cmnd[0] == MODE_SENSE) ||
	      (srb->cmnd[0] == LOG_SENSE) ||
	      (srb->cmnd[0] == MODE_SENSE_10))) {
		US_DEBUGP("-- unexpectedly short transfer\n");
		need_auto_sense = 1;
	}

	/* Now, if we need to do the auto-sense, let's do it */
	if (need_auto_sense) {
		int temp_result;
		void* old_request_buffer;
		unsigned short old_sg;
		unsigned old_request_bufflen;
		unsigned char old_sc_data_direction;
		unsigned char old_cmd_len;
		unsigned char old_cmnd[MAX_COMMAND_SIZE];
		unsigned long old_serial_number;

		US_DEBUGP("Issuing auto-REQUEST_SENSE\n");

		/* save the old command */
		memcpy(old_cmnd, srb->cmnd, MAX_COMMAND_SIZE);
		old_cmd_len = srb->cmd_len;

		/* set the command and the LUN */
		memset(srb->cmnd, 0, MAX_COMMAND_SIZE);
		srb->cmnd[0] = REQUEST_SENSE;
		srb->cmnd[1] = old_cmnd[1] & 0xE0;
		srb->cmnd[4] = 18;

		/* FIXME: we must do the protocol translation here */
		if (us->subclass == US_SC_RBC || us->subclass == US_SC_SCSI)
			srb->cmd_len = 6;
		else
			srb->cmd_len = 12;

		/* set the transfer direction */
		old_sc_data_direction = srb->sc_data_direction;
		srb->sc_data_direction = SCSI_DATA_READ;

		/* use the new buffer we have */
		old_request_buffer = srb->request_buffer;
		srb->request_buffer = srb->sense_buffer;

		/* set the buffer length for transfer */
		old_request_bufflen = srb->request_bufflen;
		srb->request_bufflen = 18;

		/* set up for no scatter-gather use */
		old_sg = srb->use_sg;
		srb->use_sg = 0;

		/* change the serial number -- toggle the high bit*/
		old_serial_number = srb->serial_number;
		srb->serial_number ^= 0x80000000;

		/* issue the auto-sense command */
		temp_result = us->transport(us->srb, us);

		/* let's clean up right away */
		srb->request_buffer = old_request_buffer;
		srb->request_bufflen = old_request_bufflen;
		srb->use_sg = old_sg;
		srb->serial_number = old_serial_number;
		srb->sc_data_direction = old_sc_data_direction;
		srb->cmd_len = old_cmd_len;
		memcpy(srb->cmnd, old_cmnd, MAX_COMMAND_SIZE);

		if (atomic_read(&us->sm_state) == US_STATE_ABORTING) {
			US_DEBUGP("-- auto-sense aborted\n");
			goto Handle_Abort;
		}
		if (temp_result != USB_STOR_TRANSPORT_GOOD) {
			US_DEBUGP("-- auto-sense failure\n");

			/* we skip the reset if this happens to be a
			 * multi-target device, since failure of an
			 * auto-sense is perfectly valid
			 */
			if (!(us->flags & US_FL_SCM_MULT_TARG))
				us->transport_reset(us);
			srb->result = DID_ERROR << 16;
			return;
		}

		US_DEBUGP("-- Result from auto-sense is %d\n", temp_result);
		US_DEBUGP("-- code: 0x%x, key: 0x%x, ASC: 0x%x, ASCQ: 0x%x\n",
			  srb->sense_buffer[0],
			  srb->sense_buffer[2] & 0xf,
			  srb->sense_buffer[12], 
			  srb->sense_buffer[13]);
#ifdef CONFIG_USB_STORAGE_DEBUG
		usb_stor_show_sense(
			  srb->sense_buffer[2] & 0xf,
			  srb->sense_buffer[12], 
			  srb->sense_buffer[13]);
#endif

		/* set the result so the higher layers expect this data */
		srb->result = CHECK_CONDITION << 1;

		/* If things are really okay, then let's show that */
		if ((srb->sense_buffer[2] & 0xf) == 0x0)
			srb->result = GOOD << 1;
	} else /* if (need_auto_sense) */
		srb->result = GOOD << 1;

	/* Regardless of auto-sense, if we _know_ we have an error
	 * condition, show that in the result code
	 */
	if (result == USB_STOR_TRANSPORT_FAILED)
		srb->result = CHECK_CONDITION << 1;

	/* If we think we're good, then make sure the sense data shows it.
	 * This is necessary because the auto-sense for some devices always
	 * sets byte 0 == 0x70, even if there is no error
	 */
	if ((us->protocol == US_PR_CB || us->protocol == US_PR_DPCM_USB) && 
	    (result == USB_STOR_TRANSPORT_GOOD) &&
	    ((srb->sense_buffer[2] & 0xf) == 0x0))
		srb->sense_buffer[0] = 0x0;
	return;

	/* abort processing: the bulk-only transport requires a reset
	 * following an abort */
	Handle_Abort:
	srb->result = DID_ABORT << 16;
	if (us->protocol == US_PR_BULK) {

		/* permit the reset transfer to take place */
		clear_bit(US_FLIDX_ABORTING, &us->flags);
		us->transport_reset(us);
	}
}

/* Stop the current URB transfer */
void usb_stor_stop_transport(struct us_data *us)
{
	US_DEBUGP("%s called\n", __FUNCTION__);

	/* If the state machine is blocked waiting for an URB,
	 * let's wake it up.  The test_and_clear_bit() call
	 * guarantees that if a URB has just been submitted,
	 * it won't be cancelled more than once. */
	if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->flags)) {
		US_DEBUGP("-- cancelling URB\n");
		usb_unlink_urb(us->current_urb);
	}

	/* If we are waiting for a scatter-gather operation, cancel it. */
	if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->flags)) {
		US_DEBUGP("-- cancelling sg request\n");
		usb_sg_cancel(us->current_sg);
	}
}

/*
 * Control/Bulk/Interrupt transport
 */

int usb_stor_CBI_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	unsigned int transfer_length = srb->request_bufflen;
	int result;

	/* COMMAND STAGE */
	/* let's send the command via the control pipe */
	result = usb_stor_ctrl_transfer(us, us->send_ctrl_pipe,
				      US_CBI_ADSC, 
				      USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 
				      us->ifnum, srb->cmnd, srb->cmd_len);

	/* check the return code for the command */
	US_DEBUGP("Call to usb_stor_ctrl_transfer() returned %d\n", result);

	/* if we stalled the command, it means command failed */
	if (result == USB_STOR_XFER_STALLED) {
		return USB_STOR_TRANSPORT_FAILED;
	}

	/* Uh oh... serious problem here */
	if (result != USB_STOR_XFER_GOOD) {
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* DATA STAGE */
	/* transfer the data payload for this command, if one exists*/
	if (transfer_length) {
		unsigned int pipe = srb->sc_data_direction == SCSI_DATA_READ ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_transfer_sg(us, pipe,
					srb->request_buffer, transfer_length,
					srb->use_sg, &srb->resid);
		US_DEBUGP("CBI data stage result is 0x%x\n", result);
		if (result == USB_STOR_XFER_ERROR)
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* STATUS STAGE */
	result = usb_stor_intr_transfer(us, us->irqdata, sizeof(us->irqdata));
	US_DEBUGP("Got interrupt data (0x%x, 0x%x)\n", 
			us->irqdata[0], us->irqdata[1]);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* UFI gives us ASC and ASCQ, like a request sense
	 *
	 * REQUEST_SENSE and INQUIRY don't affect the sense data on UFI
	 * devices, so we ignore the information for those commands.  Note
	 * that this means we could be ignoring a real error on these
	 * commands, but that can't be helped.
	 */
	if (us->subclass == US_SC_UFI) {
		if (srb->cmnd[0] == REQUEST_SENSE ||
		    srb->cmnd[0] == INQUIRY)
			return USB_STOR_TRANSPORT_GOOD;
		else {
			if (us->irqdata[0])
				return USB_STOR_TRANSPORT_FAILED;
			else
				return USB_STOR_TRANSPORT_GOOD;
		}
	}

	/* If not UFI, we interpret the data as a result code 
	 * The first byte should always be a 0x0
	 * The second byte & 0x0F should be 0x0 for good, otherwise error 
	 */
	if (us->irqdata[0]) {
		US_DEBUGP("CBI IRQ data showed reserved bType %d\n",
				us->irqdata[0]);
		return USB_STOR_TRANSPORT_ERROR;
	}

	switch (us->irqdata[1] & 0x0F) {
		case 0x00: 
			return USB_STOR_TRANSPORT_GOOD;
		case 0x01: 
			return USB_STOR_TRANSPORT_FAILED;
		default: 
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* we should never get here, but if we do, we're in trouble */
	return USB_STOR_TRANSPORT_ERROR;
}

/*
 * Control/Bulk transport
 */
int usb_stor_CB_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	unsigned int transfer_length = srb->request_bufflen;
	int result;

	/* COMMAND STAGE */
	/* let's send the command via the control pipe */
	result = usb_stor_ctrl_transfer(us, us->send_ctrl_pipe,
				      US_CBI_ADSC, 
				      USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 
				      us->ifnum, srb->cmnd, srb->cmd_len);

	/* check the return code for the command */
	US_DEBUGP("Call to usb_stor_ctrl_transfer() returned %d\n", result);

	/* if we stalled the command, it means command failed */
	if (result == USB_STOR_XFER_STALLED) {
		return USB_STOR_TRANSPORT_FAILED;
	}

	/* Uh oh... serious problem here */
	if (result != USB_STOR_XFER_GOOD) {
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* DATA STAGE */
	/* transfer the data payload for this command, if one exists*/
	if (transfer_length) {
		unsigned int pipe = srb->sc_data_direction == SCSI_DATA_READ ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_transfer_sg(us, pipe,
					srb->request_buffer, transfer_length,
					srb->use_sg, &srb->resid);
		US_DEBUGP("CB data stage result is 0x%x\n", result);
		if (result == USB_STOR_XFER_ERROR)
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* STATUS STAGE */
	/* NOTE: CB does not have a status stage.  Silly, I know.  So
	 * we have to catch this at a higher level.
	 */
	return USB_STOR_TRANSPORT_GOOD;
}

/*
 * Bulk only transport
 */

/* Determine what the maximum LUN supported is */
int usb_stor_Bulk_max_lun(struct us_data *us)
{
	unsigned char data;
	int result;

	/* issue the command */
	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				 US_BULK_GET_MAX_LUN, 
				 USB_DIR_IN | USB_TYPE_CLASS | 
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, &data, sizeof(data), HZ);

	US_DEBUGP("GetMaxLUN command result is %d, data is %d\n", 
		  result, data);

	/* if we have a successful request, return the result */
	if (result == 1)
		return data;

	/* return the default -- no LUNs */
	return 0;
}

int usb_stor_Bulk_transport(Scsi_Cmnd *srb, struct us_data *us)
{
	struct bulk_cb_wrap bcb;
	struct bulk_cs_wrap bcs;
	unsigned int transfer_length = srb->request_bufflen;
	int result;

	/* set up the command wrapper */
	bcb.Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb.DataTransferLength = cpu_to_le32(transfer_length);
	bcb.Flags = srb->sc_data_direction == SCSI_DATA_READ ? 1 << 7 : 0;
	bcb.Tag = srb->serial_number;
	bcb.Lun = srb->device->lun;
	if (us->flags & US_FL_SCM_MULT_TARG)
		bcb.Lun |= srb->device->id << 4;
	bcb.Length = srb->cmd_len;

	/* copy the command payload */
	memset(bcb.CDB, 0, sizeof(bcb.CDB));
	memcpy(bcb.CDB, srb->cmnd, bcb.Length);

	/* send it to out endpoint */
	US_DEBUGP("Bulk command S 0x%x T 0x%x Trg %d LUN %d L %d F %d CL %d\n",
		  le32_to_cpu(bcb.Signature), bcb.Tag,
		  (bcb.Lun >> 4), (bcb.Lun & 0x0F), 
		  le32_to_cpu(bcb.DataTransferLength), bcb.Flags, bcb.Length);
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				&bcb, US_BULK_CB_WRAP_LEN, NULL);
	US_DEBUGP("Bulk command transfer result=%d\n", result);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* DATA STAGE */
	/* send/receive data payload, if there is any */
	if (transfer_length) {
		unsigned int pipe = srb->sc_data_direction == SCSI_DATA_READ ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_transfer_sg(us, pipe,
					srb->request_buffer, transfer_length,
					srb->use_sg, &srb->resid);
		US_DEBUGP("Bulk data transfer result 0x%x\n", result);
		if (result == USB_STOR_XFER_ERROR)
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* See flow chart on pg 15 of the Bulk Only Transport spec for
	 * an explanation of how this code works.
	 */

	/* get CSW for device status */
	US_DEBUGP("Attempting to get CSW...\n");
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				&bcs, US_BULK_CS_WRAP_LEN, NULL);

	/* did the attempt to read the CSW fail? */
	if (result == USB_STOR_XFER_STALLED) {

		/* get the status again */
		US_DEBUGP("Attempting to get CSW (2nd try)...\n");
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				&bcs, US_BULK_CS_WRAP_LEN, NULL);
	}

	/* if we still have a failure at this point, we're in trouble */
	US_DEBUGP("Bulk status result = %d\n", result);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* check bulk status */
	US_DEBUGP("Bulk status Sig 0x%x T 0x%x R %d Stat 0x%x\n",
		  le32_to_cpu(bcs.Signature), bcs.Tag, 
		  bcs.Residue, bcs.Status);
	if ((bcs.Signature != cpu_to_le32(US_BULK_CS_SIGN) && bcs.Signature != cpu_to_le32(US_BULK_CS_OLYMPUS_SIGN)) || 
	    bcs.Tag != bcb.Tag || 
	    bcs.Status > US_BULK_STAT_PHASE) {
		US_DEBUGP("Bulk logical error\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* based on the status code, we report good or bad */
	switch (bcs.Status) {
		case US_BULK_STAT_OK:
			/* command good -- note that data could be short */
			return USB_STOR_TRANSPORT_GOOD;

		case US_BULK_STAT_FAIL:
			/* command failed */
			return USB_STOR_TRANSPORT_FAILED;

		case US_BULK_STAT_PHASE:
			/* phase error -- note that a transport reset will be
			 * invoked by the invoke_transport() function
			 */
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* we should never get here, but if we do, we're in trouble */
	return USB_STOR_TRANSPORT_ERROR;
}

/***********************************************************************
 * Reset routines
 ***********************************************************************/

/* This is the common part of the device reset code.
 *
 * It's handy that every transport mechanism uses the control endpoint for
 * resets.
 *
 * Basically, we send a reset with a 20-second timeout, so we don't get
 * jammed attempting to do the reset.
 */
static int usb_stor_reset_common(struct us_data *us,
		u8 request, u8 requesttype,
		u16 value, u16 index, void *data, u16 size)
{
	int result;
	int result2;

	/* A 20-second timeout may seem rather long, but a LaCie
	 *  StudioDrive USB2 device takes 16+ seconds to get going
	 *  following a powerup or USB attach event. */

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			request, requesttype, value, index, data, size,
			20*HZ);
	if (result < 0) {
		US_DEBUGP("Soft reset failed: %d\n", result);
		return FAILED;
	}

	/* long wait for reset, so unlock to allow disconnects */
	up(&us->dev_semaphore);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ*6);
	set_current_state(TASK_RUNNING);
	down(&us->dev_semaphore);

	US_DEBUGP("Soft reset: clearing bulk-in endpoint halt\n");
	result = usb_stor_clear_halt(us, us->recv_bulk_pipe);

	US_DEBUGP("Soft reset: clearing bulk-out endpoint halt\n");
	result2 = usb_stor_clear_halt(us, us->send_bulk_pipe);

	/* return a result code based on the result of the control message */
	if (result < 0 || result2 < 0) {
		US_DEBUGP("Soft reset failed\n");
		return FAILED;
	}
	US_DEBUGP("Soft reset done\n");
	return SUCCESS;
}

/* This issues a CB[I] Reset to the device in question
 */
int usb_stor_CB_reset(struct us_data *us)
{
	unsigned char cmd[12];

	US_DEBUGP("CB_reset() called\n");

	memset(cmd, 0xFF, sizeof(cmd));
	cmd[0] = SEND_DIAGNOSTIC;
	cmd[1] = 4;
	return usb_stor_reset_common(us, US_CBI_ADSC, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, cmd, sizeof(cmd));
}

/* This issues a Bulk-only Reset to the device in question, including
 * clearing the subsequent endpoint halts that may occur.
 */
int usb_stor_Bulk_reset(struct us_data *us)
{
	US_DEBUGP("Bulk reset requested\n");

	return usb_stor_reset_common(us, US_BULK_RESET_REQUEST, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, NULL, 0);
}
