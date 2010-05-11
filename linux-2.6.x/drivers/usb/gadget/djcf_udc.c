/*
 * djcf_udc -- driver for Coldfire Deskjet ASIC USB controller
 *
 *   Copyright (C) 2010 by Brian S. Julin
 *
 * Based on at91_udc.c
 *   Copyright (C) 2004 by Thomas Rathbone
 *   Copyright (C) 2005 by HP Labs
 *   Copyright (C) 2005 by David Brownell
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

/**
 * DOC:
 *
 * The USB controller on Coldfire-based deskjet is a simple unit
 * supporting ep0in/ep0out, one additional in endpoint, and one
 * additional out endpoint.
 *
 * The controller mostly self-configures by snooping initial packets
 * for example, it will assume the correct device ID without
 * explicitly being told to do so.  After that, it handles a few
 * of the more tedious tasks, but passes most traffic up the stack,
 * so there is no need to synthesize events for the higher layers.
 *
 * This driver is non-SMP and likely always will be.
 *
 * Though the deskjet does have a DMA-like function, it has not been
 * explored yet.  This driver uses MMIO only.
 *
 * Some elements from at91_udc.c are left in for stuff that may be
 * implemented in the future: e.g. dma, suspend.
 *
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/dj/djio.h>
#include <asm/dj/usb.h>
#include <asm/dj/irq.h>

#define	NUM_ENDPOINTS	3

struct djcf_ep {
	struct usb_ep			ep;
	struct list_head		queue;
	struct djcf_udc			*udc;
	u8				reg_offset;
	u8 __iomem			*stat;
	u8 __iomem			*cntl;
	u8 __iomem			*flsh;
	u16 __iomem			*dreg;

	unsigned			maxpacket:16;
	unsigned			is_pingpong:1;

	unsigned			stopped:1;
	unsigned			is_in:1;
	unsigned			is_iso:1;

	const struct usb_endpoint_descriptor *desc;
};

struct djcf_udc {
	struct usb_gadget		gadget;
	struct djcf_ep			ep[NUM_ENDPOINTS];
	struct usb_gadget_driver	*driver;
	u8				usb_addr;
	u8				int_mask;
	unsigned			enabled:1;
	unsigned			clocked:1;
	unsigned			suspended:1;
	unsigned			req_pending:1;
	unsigned			active_suspend:1;
};

struct djcf_request {
	struct usb_request		req;
	struct list_head		queue;
};

static inline struct djcf_udc *to_udc(struct usb_gadget *g)
{
	return container_of(g, struct djcf_udc, gadget);
}

static inline u8 mask_irqs(struct djcf_udc *udc, u8 mask) {
	u8 ret;
	ret = udc->int_mask;
	writeb(mask, DJIO_A_USB + DJIO_A_USB_IRQA_ENAB);
	udc->int_mask = mask;
	return ret;
}

/*-------------------------------------------------------------------------*/

#ifdef VERBOSE_DEBUG
#    define VDBG		DBG
#else
#    define VDBG(stuff...)	do{}while(0)
#endif

#ifdef PACKET_TRACE
#    define PACKET		VDBG
#else
#    define PACKET(stuff...)	do{}while(0)
#endif

#define ERR(stuff...)		printk("udc: " stuff)
#define WARNING(stuff...)	printk("udc: " stuff)
#define INFO(stuff...)		printk("udc: " stuff)
#define DBG(stuff...)		printk("udc: " stuff)

#define	DRIVER_VERSION	"10 Feb 2010"

static const char driver_name [] = "djcf_udc";
static const char ep0name[] = "ep0";

/*-------------------------------------------------------------------------*/

static void done(struct djcf_ep *ep, struct djcf_request *req, int status)
{
	unsigned	stopped = ep->stopped;

	list_del_init(&req->queue);
	if (req->req.status == -EINPROGRESS)
		req->req.status = status;
	else
		status = req->req.status;
	if (status && status != -ESHUTDOWN)
		VDBG("%s done %p, status %d\n", ep->ep.name, req, status);

	ep->stopped = 1;
	req->req.complete(&ep->ep, &req->req);
	ep->stopped = stopped;
}

/*-------------------------------------------------------------------------*/

/* pull OUT packet data from the endpoint's fifo */
static int read_fifo (struct djcf_ep *ep, struct djcf_request *req)
{
	u8		csr;
	s16		tmp;
	int		got;
	u8		*buf;
	unsigned int	count;
	unsigned int	bufferspace;
	unsigned int	is_done;

	buf = req->req.buf + req->req.actual;
	bufferspace = req->req.length - req->req.actual;

	csr = readb(ep->stat);
	if (csr & 1)
		return 0;

	count = ep->ep.maxpacket;
	if (count > bufferspace)
		count = bufferspace;

	for (got = 0; got < count; got++) {
		tmp = readw(ep->dreg);
		if (tmp < 0) break;
		buf[got] = tmp & 0xff;
	}
	writeb(11, ep->cntl);
	writeb(3, ep->cntl);
	if ((got == count) && (count < ep->ep.maxpacket)) {
		DBG("%s buffer overflow\n", ep->ep.name);
		req->req.status = -EOVERFLOW;
		count = bufferspace;
	}

	req->req.actual += got;
	is_done = (got < ep->ep.maxpacket);
	if (got == bufferspace)
		is_done = 1;

	PACKET("%s %p out/%d%s\n", ep->ep.name, &req->req, got,
			is_done ? " (done)" : "");

	if (is_done)
		done(ep, req, 0);

	return is_done;
}

/* load fifo for an IN packet */
static int write_fifo(struct djcf_ep *ep, struct djcf_request *req)
{
	u8		csr = readb(ep->stat);
	unsigned	total, count, is_last;
	u8		*buf;
	int		tmp;

	if (csr & 2) {
		writeb(0, ep->flsh);
		writeb(3, ep->cntl);
		return 0;
	}
	if (!(csr & 1)) {
		return 0;
	}

	buf = req->req.buf + req->req.actual;
	prefetch(buf);
	total = req->req.length - req->req.actual;
	if (ep->ep.maxpacket < total) {
		count = ep->ep.maxpacket;
		is_last = 0;
	} else {
		count = total;
		is_last = (count < ep->ep.maxpacket) || !req->req.zero;
	}

	writeb(1, ep->cntl);
	for (tmp = 0; tmp < count; tmp++) {
		csr = readb(ep->stat);
		if (csr & 2) {
			break;
		}
		writeb(buf[tmp],ep->dreg);
	}
	if (ep->reg_offset == 1) {
		writeb(11, DJIO_A_USB + DJIO_A_EP0_R_CNTL);
		writeb(3, DJIO_A_USB + DJIO_A_EP0_R_CNTL);
	}
	writeb(0, ep->flsh);
	writeb(3, ep->cntl);
	if (tmp < count) {
		count = tmp;
                /* TODO: recondition hw */
		done(ep, req, -ECONNRESET);
		return -1;
	}
	req->req.actual += count;

	PACKET("%s %p in/%d%s\n", ep->ep.name, &req->req, count,
			is_last ? " (done)" : "");
	if (is_last)
		done(ep, req, 0);
	return is_last;
}

static void nuke(struct djcf_ep *ep, int status)
{
	struct djcf_request *req;

	ep->stopped = 1;
	if (list_empty(&ep->queue))
		return;

	VDBG("%s %s\n", __func__, ep->ep.name);
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next, struct djcf_request, queue);
		done(ep, req, status);
	}
}

/*-------------------------------------------------------------------------*/

static int ep_enable(struct usb_ep *_ep,
				const struct usb_endpoint_descriptor *desc)
{
	struct djcf_ep	*ep = container_of(_ep, struct djcf_ep, ep);
	struct djcf_udc	*dev = ep->udc;
	u16		maxpacket;
	u32		tmp;
	unsigned long	flags;

	if (!ep || !_ep || !desc || ep->desc
	    	|| _ep->name == ep0name
	    	|| desc->bDescriptorType != USB_DT_ENDPOINT
	    	|| (maxpacket = le16_to_cpu(desc->wMaxPacketSize)) == 0
	    	|| maxpacket > ep->maxpacket) {
		DBG("bad ep or descriptor\n");
		return -EINVAL;
	}

	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN) {
		DBG("bogus device state\n");
		return -ESHUTDOWN;
	}

	tmp = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	switch (tmp) {
	case USB_ENDPOINT_XFER_CONTROL:
		DBG("only one control endpoint\n");
		return -EINVAL;
	case USB_ENDPOINT_XFER_INT:
		if (maxpacket > 64)
			goto bogus_max;
		break;
	case USB_ENDPOINT_XFER_BULK:
		switch (maxpacket) {
		case 8:
		case 16:
		case 32:
		case 64:
			goto ok;
		}
bogus_max:
		DBG("bogus maxpacket %d\n", maxpacket);
		return -EINVAL;
	case USB_ENDPOINT_XFER_ISOC:
		if (!ep->is_pingpong) {
			DBG("iso requires double buffering\n");
			return -EINVAL;
		}
		break;
	}

ok:
	local_irq_save(flags);

	/* initialize endpoint to match this descriptor */
	ep->is_in = (desc->bEndpointAddress & USB_DIR_IN) != 0;
	ep->is_iso = (tmp == USB_ENDPOINT_XFER_ISOC);
	ep->stopped = 0;
	if (ep->is_in)
		tmp |= 0x04;

	ep->desc = desc;
	ep->ep.maxpacket = maxpacket;

	/*
	 * reset/init endpoint fifo.
	 */
	writeb(11, ep->cntl);
	writeb(3, ep->cntl);

	local_irq_restore(flags);
	return 0;
}

static int ep_disable (struct usb_ep * _ep)
{
	struct djcf_ep	*ep = container_of(_ep, struct djcf_ep, ep);
	unsigned long	flags;

	if (ep == &ep->udc->ep[0])
		return -EINVAL;

	local_irq_save(flags);

	nuke(ep, -ESHUTDOWN);

	/* restore the endpoint's pristine config */
	ep->desc = NULL;
	ep->ep.maxpacket = ep->maxpacket;

	/* reset fifos and endpoint */
	if (ep->udc->clocked) {
		writeb(0, ep->flsh);
		writeb(11, ep->cntl);
	}

	local_irq_restore(flags);
	return 0;
}


static struct usb_request *ep_alloc_request(struct usb_ep *_ep, gfp_t gfp_flags)
{
	struct djcf_request *req;

	req = kzalloc(sizeof(*req), gfp_flags);
	if (!req)
		return NULL;

	INIT_LIST_HEAD(&req->queue);
	return &req->req;
}


static void ep_free_request(struct usb_ep *_ep, struct usb_request *_req)
{
	struct djcf_request *req;

	req = container_of(_req, struct djcf_request, req);
	BUG_ON(!list_empty(&req->queue));
	kfree(req);
}

static int ep_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t unused)
{
	struct djcf_request	*req;
	struct djcf_ep		*ep;
	struct djcf_udc		*dev;
	int			status;
	unsigned long		flags;
	u8			int_mask;

	req = container_of(_req, struct djcf_request, req);
	if (!_req ||
	    !_req->complete || !_req->buf || !list_empty(&req->queue)) {
		DBG("invalid request\n");
		return -EINVAL;
	}

	ep = container_of(_ep, struct djcf_ep, ep);
	if (!_ep || (!ep->desc && ep->ep.name != ep0name)) {
		DBG("invalid ep\n");
		return -EINVAL;
	}

	dev = ep->udc;
	if (!dev || !dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN) {
		DBG("invalid device\n");
		return -EINVAL;
	}

	_req->status = -EINPROGRESS;
	_req->actual = 0;

	local_irq_save(flags);
	int_mask = mask_irqs(ep->udc, 0);
	local_irq_restore(flags);

	/* try to kickstart any empty and idle queue */
	if (list_empty(&ep->queue) && !ep->stopped) {
		int	is_ep0;
		/*
		 * If this control request has a non-empty DATA stage, this
		 * will start that stage.  It works just like a non-control
		 * request (until the status stage starts, maybe early).
		 *
		 * If the data stage is empty, then this starts a successful
		 * IN/STATUS stage.  (Unsuccessful ones use set_halt.)
		 */
		is_ep0 = (ep->ep.name == ep0name);
		if (is_ep0) {

			if (!dev->req_pending) {
				status = -EINVAL;
				goto done;
			}

			if (req->req.length == 0) {
ep0_in_status:
				PACKET("ep0 in/status\n");
				status = 0;
				dev->req_pending = 0;
				goto done;
			}
		}

		if (ep->is_in) {
			status = write_fifo(ep, req);
		}
		else {
			status = read_fifo(ep, req);
			/* IN/STATUS stage is otherwise triggered by irq */
			if (status && is_ep0)
				goto ep0_in_status;
		}
	} else {
		status = 0;
	}

	if (req && !status)
		list_add_tail (&req->queue, &ep->queue);
done:
	local_irq_save(flags);
	int_mask = mask_irqs(ep->udc, int_mask);
	local_irq_restore(flags);
	return (status < 0) ? status : 0;
}

static int ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct djcf_ep *ep;
	struct djcf_request *req;

	ep = container_of(_ep, struct djcf_ep, ep);
	if (!_ep || ep->ep.name == ep0name)
		return -EINVAL;

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry (req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}
	if (&req->req != _req)
		return -EINVAL;

	done(ep, req, -ECONNRESET);
	return 0;
}


static int ep_set_halt(struct usb_ep *_ep, int value)
{
	struct djcf_ep	*ep = container_of(_ep, struct djcf_ep, ep);
	u8		csr;
	unsigned long	flags;
	int		status = 0;

	if (!_ep || ep->is_iso || !ep->udc->clocked)
		return -EINVAL;

	local_irq_save(flags);

	csr = readb(ep->stat);
	if (ep->reg_offset == 1) {
		/* ep0 write FIFO has a strange bit inversion. */
		csr ^= 0x1;
	}

	/*
	 * fail with still-busy IN endpoints, ensuring correct sequencing
	 * of data tx then stall.  note that the fifo rx bytecount isn't
	 * completely accurate as a tx bytecount.
	 */
	if (ep->is_in && (!list_empty(&ep->queue) || (csr & 1)))
		status = -EAGAIN;
	else {
		writeb(11, ep->cntl);
	}

	local_irq_restore(flags);
	return status;
}


static const struct usb_ep_ops djcf_ep_ops = {
	.enable		= ep_enable,
	.disable	= ep_disable,
	.alloc_request	= ep_alloc_request,
	.free_request	= ep_free_request,
	.queue		= ep_queue,
	.dequeue	= ep_dequeue,
	.set_halt	= ep_set_halt,
};


/*-------------------------------------------------------------------------*/


static int djcf_get_frame(struct usb_gadget *gadget)
{
	if (!to_udc(gadget)->clocked)
		return -EINVAL;
	return -ENOTSUPP;
}


static int djcf_wakeup(struct usb_gadget *gadget)
{
	struct djcf_udc	*udc = to_udc(gadget);
	int		status = -EINVAL;
	unsigned long	flags;
	u16		tmp;

	DBG("%s\n", __func__ );
	local_irq_save(flags);

	if (!udc->clocked || !udc->suspended)
		goto done;

	writeb(1, DJIO_A_USB + 0x0c);  /* (???) */
	writew(1, DJIO_A_UIFP_USBOLD_CTL2);
	writew(1, DJIO_A_UIFP_USBOLD_CTL1);

	tmp = readw(DJIO_B_SYS_REV);
	if (DJIO_B_SYS_REV_TEST(tmp)) {
		writew(0,DJIO_A_UIFP_USBOLD_CTL2);
		tmp = readw(DJIO_A_UIFP_USBNEW_CTL1) | 1;
		writew(tmp,DJIO_A_UIFP_USBNEW_CTL1);
	}

	writeb(11,   DJIO_A_USB + DJIO_A_EP2_R_CNTL);
	writeb(11,   DJIO_A_USB + DJIO_A_EP1_W_CNTL);
	writeb(3,    DJIO_A_USB + DJIO_A_EP2_R_CNTL);
	writeb(3,    DJIO_A_USB + DJIO_A_EP1_W_CNTL);
	writeb(11,   DJIO_A_USB + DJIO_A_EP0_R_CNTL);
	writeb(3,    DJIO_A_USB + DJIO_A_EP0_R_CNTL);
	writeb(8,    DJIO_A_USB + DJIO_A_EP0_W_CNTL);
	writeb(1,    DJIO_A_USB + DJIO_A_EP0_W_CNTL);
	writeb(DJIO_A_USB_IRQM_ALL, DJIO_A_USB + DJIO_A_USB_IRQA_ACK);
done:
	local_irq_restore(flags);
	return status;
}


static void udc_reinit(struct djcf_udc *udc)
{
	u32 i;

	INIT_LIST_HEAD(&udc->gadget.ep_list);
	INIT_LIST_HEAD(&udc->gadget.ep0->ep_list);

	for (i = 0; i < NUM_ENDPOINTS; i++) {
		struct djcf_ep *ep = &udc->ep[i];

		if (i != 0)
			list_add_tail(&ep->ep.ep_list, &udc->gadget.ep_list);
		ep->desc = NULL;
		ep->stopped = 0;
		ep->ep.maxpacket = ep->maxpacket;

		ep->reg_offset = i + 1;
		ep->stat = (u8 __iomem *)(DJIO_A_USB + DJIO_A_EP0_W_STAT) + i;
		ep->cntl = (u8 __iomem *)(DJIO_A_USB + DJIO_A_EP0_W_CNTL) + i;
		ep->flsh = (u8 __iomem *)(DJIO_A_USB + DJIO_A_EP0_W_FLSH) + i;
		ep->dreg = (u16 __iomem *)(DJIO_C_USB + DJIO_C_USB_EP0_W) + i;
		INIT_LIST_HEAD(&ep->queue);
	}
}

#if 0

/* Saved for when we get to power saving stuff */

static void stop_activity(struct djcf_udc *udc)
{
	struct usb_gadget_driver *driver = udc->driver;
	int i;

	if (udc->gadget.speed == USB_SPEED_UNKNOWN)
		driver = NULL;
	udc->gadget.speed = USB_SPEED_UNKNOWN;
	udc->suspended = 0;

	for (i = 0; i < NUM_ENDPOINTS; i++) {
		struct djcf_ep *ep = &udc->ep[i];
		ep->stopped = 1;
		nuke(ep, -ESHUTDOWN);
	}
	if (driver)
		driver->disconnect(&udc->gadget);

	udc_reinit(udc);
}

#endif

static const struct usb_gadget_ops djcf_udc_ops = {
	.get_frame		= djcf_get_frame,
	.wakeup			= djcf_wakeup,
};

/*-------------------------------------------------------------------------*/

union setup {
	u8			raw[8];
	struct usb_ctrlrequest	r;
};

static void handle_setup(struct djcf_udc *udc, struct djcf_ep *ep, u8 csr)
{
	s16		tmp;
	union setup	pkt;
	int		status = 0;
	int		got;

 again:

	csr = readb(DJIO_A_USB + DJIO_A_EP0_R_STAT);
	if (csr & 1)
		return;

	for (got = 0; got < 8; got++) {
		tmp = readw(DJIO_C_USB + DJIO_C_USB_EP0_R);
		if (tmp < 0) break;
		pkt.raw[got] = tmp & 0xff;
	}
	if (got == 8) {
		if (pkt.r.bRequestType & USB_DIR_IN) {
			writeb(11, DJIO_A_USB + DJIO_A_EP0_R_CNTL);
			writeb(3, DJIO_A_USB + DJIO_A_EP0_R_CNTL);
			writeb(8, DJIO_A_USB + DJIO_A_EP0_W_CNTL);
			writeb(1, DJIO_A_USB + DJIO_A_EP0_W_CNTL);
			ep->is_in = 1;
		} else {
			writeb(8,DJIO_A_USB + DJIO_A_EP0_W_CNTL);
			ep->is_in = 0;
		}
	} else {
		writeb(8, DJIO_A_USB + DJIO_A_EP0_W_CNTL);
		ERR("SETUP len %d, csr %08x\n", got, csr);
		status = -EINVAL;
	}
	ep->stopped = 0;
	if (status != 0)
		goto stall;

	VDBG("SETUP %02x.%02x v%04x i%04x l%04x\n",
	     pkt.r.bRequestType, pkt.r.bRequest,
	     le16_to_cpu(pkt.r.wValue),
	     le16_to_cpu(pkt.r.wIndex),
	     le16_to_cpu(pkt.r.wLength));

	udc->req_pending = 1;

	if (pkt.r.bRequestType == (USB_TYPE_STANDARD | USB_RECIP_DEVICE)) {
		if (pkt.r.bRequest == USB_REQ_SET_ADDRESS) {
			udc->usb_addr = le16_to_cpu(pkt.r.wValue);
			udc->req_pending = 0;
			goto again;
		}
	}

	/* pass request up to the gadget driver */
	if (udc->driver)
		status = udc->driver->setup(&udc->gadget, &pkt.r);
	else
		status = -ENODEV;
	if (status < 0) {
stall:
		VDBG("req %02x.%02x protocol STALL; stat %d\n",
				pkt.r.bRequestType, pkt.r.bRequest, status);
		nuke(&udc->ep[0], -EPROTO);
		udc->req_pending = 0;
	}
	if (!(pkt.r.bRequestType & USB_DIR_IN)) goto again;
	return;
}


static int handle_ep(struct djcf_ep *ep)
{
	struct djcf_request *req;

	req = NULL;
	if (!list_empty(&ep->queue))
		req = list_entry(ep->queue.next, struct djcf_request, queue);
	if (!req)
		return 0;

	if (ep->is_in) {
		return write_fifo(ep, req);
	} else {
		return read_fifo(ep, req);
	}

	return 0;
}


static void handle_ep0(struct djcf_udc *udc)
{
	struct djcf_ep		*ep0in = &udc->ep[0];
	u8 __iomem		*stat = ep0in->stat;
	u8			csr = readb(stat);
	struct djcf_request	*req;

	if (csr & 2) {
		nuke(ep0in, -EPROTO);
		udc->req_pending = 0;
		VDBG("ep0 stalled\n");
		return;
	}
	if (!(readb(DJIO_A_USB + DJIO_A_EP0_W_STAT) & 1)) {
		if (!(readb(DJIO_A_USB + DJIO_A_EP0_R_STAT) & 1)) {
			/* Got a setup */
			nuke(ep0in, 0);
			udc->req_pending = 0;
			goto tryout;
		}
	}

	if (list_empty(&ep0in->queue))
		req = NULL;
	else
		req = list_entry(ep0in->queue.next, struct djcf_request, queue);

	/* write more IN DATA? */
	if (req && ep0in->is_in) {
		if (handle_ep(ep0in))
			udc->req_pending = 0;
	}
	if (!(readb(DJIO_A_USB + DJIO_A_EP0_R_STAT) & 1)) {
 tryout:
		handle_setup(udc, ep0in, csr);
	}
}

/*-------------------------------------------------------------------------*/

static struct djcf_udc djudc = {
	.gadget = {
		.ops	= &djcf_udc_ops,
		.ep0	= &djudc.ep[0].ep,
		.name	= driver_name,
		.speed  = USB_SPEED_FULL,
		.dev	= {
			.init_name = "gadget",
			.parent = &platform_bus,
		}
	},
	.ep[0] = {
		/* 
		 * This is ep0in -- ep0out is not advertised and
		 * is handled implicitly.  Kludge-ass standard.
		 */
		.ep = {
			.name	= ep0name,
			.ops	= &djcf_ep_ops,
		},
		.udc		= &djudc,
		.maxpacket	= 64,
	},
	.ep[1] = {
		.ep = {
			.name	= "ep2out-bulk",
			.ops	= &djcf_ep_ops,
		},
		.udc		= &djudc,
		.maxpacket	= 64,
	},
	.ep[2] = {
		.ep = {
			.name	= "ep1in-bulk",
			.ops	= &djcf_ep_ops,
		},
		.udc		= &djudc,
		.maxpacket	= 64,
	}
};


/*----------------------------------------------------------------------------*/

/* IRQ handler */

/* We use this irq source to jump start.  It seems to always fire. */
static int turned_off_src2 = 0;

static irqreturn_t djcf_udc_irq (int irq, void *unused)
{
	u32			rescans = 4;
	u8			irqsrc;
	u8			irq_mask;
	unsigned long flags;

	local_irq_save(flags);
	/* Turn off IRQs at the source */
	irq_mask = mask_irqs(&djudc, 0);
	/* Ack the line */
	writeb(2, DJIO_A_IRQ_N(DJIO_A_USB_IRQA_LINE));
	/* Figure out what caused the irq. */
	irqsrc = readb(DJIO_A_USB + DJIO_A_USB_IRQA_ACK);
	/* Ack the source */
	writeb(0x1f, DJIO_A_USB + DJIO_A_USB_IRQA_ACK);
	/* Let other stuff punch through */
	//	local_irq_enable_in_hardirq();

	/* We mostly ignore these. */
	if (irqsrc & (0x2 | DJIO_A_USB_IRQM_L1)) {
		if (irqsrc & 2) {
			/* This should happen just once during jump start. */
			if (!turned_off_src2) {
				irq_mask = 0x1d;
				turned_off_src2 = 1;
			}
			else {
				BUG();
			}
		}
		if (irqsrc & 0x10)
			goto leave;
		if (!(irqsrc & 0x5))
			goto leave;
	}

	while (rescans--) {
		struct djcf_ep *ep = &djudc.ep[1];
		
		handle_ep0(&djudc);
		while (ep < &djudc.ep[NUM_ENDPOINTS])
			handle_ep(ep++);
	}

 leave:
	mask_irqs(&djudc, irq_mask);
	local_irq_restore(flags);

	return IRQ_HANDLED;
}


int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	int		ret;

	if (!driver || !driver->bind || !driver->setup
	    || driver->speed < USB_SPEED_FULL) {
		DBG("bad parameter.\n");
		return -EINVAL;
	}

	if (djudc.driver) {
		DBG("UDC already has a gadget driver\n");
		return -EBUSY;
	}

	djudc.driver = driver;
	djudc.gadget.dev.driver = &driver->driver;
	djudc.gadget.dev.driver_data = &driver->driver;
	djudc.enabled = 1;

	udc_reinit(&djudc);
	djudc.clocked = 1;
	djudc.suspended = 1;
	djcf_wakeup(&djudc.gadget);
	djudc.clocked = 0;
	djudc.suspended = 0;

	ret = driver->bind(&djudc.gadget);
	if (ret) {
		DBG("driver->bind() returned %d\n", ret);
		djudc.driver = NULL;
		djudc.gadget.dev.driver = NULL;
		djudc.gadget.dev.driver_data = NULL;
		djudc.enabled = 0;
		return ret;
	}

	DBG("bound to %s\n", driver->driver.name);

	/* IRQ will fire to start us up */
	djudc.int_mask = 0xff;
	writeb(0xff, DJIO_A_USB + DJIO_A_USB_IRQA_ENAB);
	writeb(0xff, DJIO_A_USB + DJIO_A_USB_IRQA_ACK);

	return 0;
}
EXPORT_SYMBOL (usb_gadget_register_driver);

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	if (!driver || driver != djudc.driver || !driver->unbind)
		return -EINVAL;

	local_irq_disable();
	djudc.enabled = 0;
	local_irq_enable();

	driver->unbind(&djudc.gadget);
	djudc.gadget.dev.driver = NULL;
	djudc.gadget.dev.driver_data = NULL;
	djudc.driver = NULL;

	djcf_wakeup(&djudc.gadget);

	DBG("unbound from %s\n", driver->driver.name);
	return 0;
}
EXPORT_SYMBOL (usb_gadget_unregister_driver);

static struct irqaction djcf_udc_irqaction = {
	.name    = "usb",
	.flags   = 0,
	.handler = djcf_udc_irq,
};

static int __init udc_init_module(void)
{
	setup_irq(DJIO_IRQ_BASE + DJIO_A_USB_IRQA_LINE, &djcf_udc_irqaction);
	device_register(&djudc.gadget.dev);
	return 0;
}
module_init(udc_init_module);

static void __exit udc_exit_module(void)
{
	device_unregister(&djudc.gadget.dev);
	remove_irq(DJIO_IRQ_BASE + DJIO_A_USB_IRQA_LINE, &djcf_udc_irqaction);
}
module_exit(udc_exit_module);

MODULE_DESCRIPTION("ColdFire DeskJet udc driver");
MODULE_AUTHOR("Brian S. Julin");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:djcf_udc");
