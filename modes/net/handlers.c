/*
  libxbee - a C library to aid the use of Digi's XBee wireless modules
            running in API mode (AP=2).

  Copyright (C) 2009  Attie Grande (attie@attie.co.uk)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../internal.h"
#include "../../xbee_int.h"
#include "../../mode.h"
#include "../../conn.h"
#include "../../pkt.h"
#include "../../ll.h"
#include "../../net_handlers.h"
#include "handlers.h"

xbee_err xbee_net_frontchannel_rx_func(struct xbee *xbee, void *arg, unsigned char identifier, struct xbee_buf *buf, struct xbee_frameInfo *frameInfo, struct xbee_conAddress *address, struct xbee_pkt **pkt) {
	struct xbee_pkt *iPkt;
	xbee_err ret;
	int i;
	int pos;
	struct xbee_modeConType *conType;
	struct xbee_con *con;
	
	if (buf->len < 3) return XBEE_ELENGTH;
	
	conType = NULL;
	for (i = 0; xbee->iface.conTypes[i].name; i++) {
		if (!xbee->iface.conTypes[i].rxHandler) continue;
		if (xbee->iface.conTypes[i].rxHandler->identifier != identifier) continue;
		conType = &(xbee->iface.conTypes[i]);
		break;
	}
	if (!conType) return XBEE_EINVAL;
	
	for (con = NULL; ll_get_next(conType->conList, con, (void **)&con) == XBEE_ENONE && con; ) {
		if (con->conIdentifier == (((buf->data[1] << 8) & 0xFF) | (buf->data[2] & 0xFF))) break;
	}
	if (!con) return XBEE_EINVAL;
	
	printf("got me a con @ %p\n", con);
	
	/* steal it's address */
	memcpy(address, &con->address, sizeof(*address));
	
	pos = 3;
	if ((ret = xbee_pktAlloc(&iPkt, NULL, buf->len - pos)) != XBEE_ENONE) return ret;
	
	iPkt->dataLen = buf->len - pos;
	iPkt->status = buf->data[pos];                           pos++;
	iPkt->settings = buf->data[pos];                         pos++;
	iPkt->rssi = buf->data[pos];                             pos++;
	iPkt->frameId = buf->data[pos];                          pos++;
	iPkt->atCommand[0] = buf->data[pos];                     pos++;
	iPkt->atCommand[1] = buf->data[pos];                     pos++;
	
	if (iPkt->dataLen > 0) {
		memcpy(iPkt->data, &buf->data[pos], iPkt->dataLen);
	}
	
	*pkt = iPkt;
	
	return XBEE_ENONE;
}

xbee_err xbee_net_frontchannel_tx_func(struct xbee *xbee, struct xbee_con *con, void *arg, unsigned char identifier, unsigned char frameId, struct xbee_conAddress *address, struct xbee_conSettings *settings, unsigned char *buf, int len, struct xbee_buf **oBuf) {
	struct xbee_buf *iBuf;
	size_t bufLen;
	size_t memSize;
	int pos;
	
	/* identifier + frameId + conId (2 bytes) */
	memSize = 4 + len;
	bufLen = memSize;
	
	memSize += sizeof(*iBuf);
	
	if ((iBuf = malloc(memSize)) == NULL) return XBEE_ENOMEM;
	
	pos = 0;
	iBuf->len = bufLen;
	iBuf->data[pos] = identifier;                         pos++;
	iBuf->data[pos] = frameId;                            pos++;
	iBuf->data[pos] = (con->conIdentifier >> 8) & 0xFF;   pos++;
	iBuf->data[pos] = con->conIdentifier & 0xFF;          pos++;
	memcpy(&(iBuf->data[pos]), buf, len);                 pos += len;
	
	*oBuf = iBuf;
	
	return XBEE_ENONE;
}

/* ######################################################################### */

/* these are allocated at runtime - this is just for reference...

const struct xbee_modeDataHandlerRx xbee_net_frontchannel_rx = {
	.identifier = 0x00,
	.func = xbee_net_frontchannel_rx_func,
};

const struct xbee_modeDataHandlerTx xbee_net_frontchannel_tx = {
	.identifier = 0x00,
	.func = xbee_net_frontchannel_tx_func,
};
*/

/* this one allows frame IDs */
const struct xbee_modeConType xbee_net_frontchannel_template = {
	.name = NULL,
	.internal = 0,
	.allowFrameId = 1,
	.useTimeout = 1,
	.timeout = {
		.tv_sec = 5,
		.tv_nsec = 0,
	},
	.rxHandler = NULL,
	.txHandler = NULL,
};

/* this one does not allow frame IDs */
const struct xbee_modeConType xbee_net_frontchannel_template_fid = {
	.name = NULL,
	.internal = 0,
	.allowFrameId = 1,
	.useTimeout = 1,
	.timeout = {
		.tv_sec = 5,
		.tv_nsec = 0,
	},
	.rxHandler = NULL,
	.txHandler = NULL,
};

/* ######################################################################### */

const struct xbee_modeConType xbee_net_backchannel = {
	.name = "Backchannel",
	.internal = 1,
	.allowFrameId = 1, /* this needs redeclaring, because this is enabled for the client */
	.useTimeout = 1,
	.timeout = {
		.tv_sec = 5,
		.tv_nsec = 0,
	},
	.rxHandler = &xbee_netServer_backchannel_rx,
	.txHandler = &xbee_netServer_backchannel_tx,
};
