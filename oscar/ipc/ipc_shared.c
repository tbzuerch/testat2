/*	Oscar, a hardware abstraction framework for the LeanXcam and IndXcam.
	Copyright (C) 2008 Supercomputing Systems AG
	
	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.
	
	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
	General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*! @file
 * @brief Interprocess communication module implementation shared
 * part for host and target
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "ipc.h"

/*! The camera module singelton instance. */
struct OSC_IPC ipc;

OSC_ERR OscIpcCreate();
OSC_ERR OscIpcDestroy();

struct OscModule OscModule_ipc = {
	.name = "ipc",
	.create = OscIpcCreate,
	.destroy = OscIpcDestroy,
	.dependencies = {
		&OscModule_log,
		NULL // To end the flexible array.
	}
};

OSC_ERR OscIpcCreate()
{
	ipc = (struct OSC_IPC) { };
	
	return SUCCESS;
}

OSC_ERR OscIpcDestroy()
{
	OSC_IPC_CHAN_ID i;

	/* Close all sockets associated with open channels */
	for(i = 0; i < MAX_NR_IPC_CHANNELS; i++)
		if(ipc.arybIpcChansBusy[i] == TRUE)
			if(ipc.aryIpcChans[i].sock > 0)
				OscIpcUnregisterChannel(ipc.aryIpcChans[i].sock);
	
	return SUCCESS;
}

// FUXME: WTF for is this function?
static inline void UniqueClientSocketName(char *strSocketPath)
{
	sprintf(strSocketPath,
			SOCKET_PATH "%d.sock",
			getpid());
}

OSC_ERR OscIpcRegisterChannel(
		OSC_IPC_CHAN_ID * pIpcChan,
		const char *strSocketPath,
		const int flags)
{
	unsigned int        sock;
	struct sockaddr_un  addr;
	int                 len, ret, chan;

	if(unlikely((pIpcChan == NULL) ||
			(strSocketPath == NULL) || (strSocketPath[0] == '\0')))
	{
		OscLog(ERROR, "%s(0x%x, 0x%x): Invalid parameter!\n",
				__func__, pIpcChan, strSocketPath);
		return -EINVALID_PARAMETER;
	}
	
	/* Find a free IPC channel */
	for(chan = 0; chan < MAX_NR_IPC_CHANNELS; chan++)
	{
		if(ipc.arybIpcChansBusy[chan] == FALSE)
		{
			break;
		}
	}
	if(unlikely(chan == MAX_NR_IPC_CHANNELS))
	{
		OscLog(ERROR, "%s: All IPC channels busy!\n", __func__);
		return -EDEVICE_BUSY;
	}
		
	/* Create the socket. */
	ipc.aryIpcChans[chan].sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sock < 0)
	{
		OscLog(ERROR, "%s: Error allocating socket! (%s)\n",
				__func__,
				strerror(errno));
		return -ESOCKET;
	}

	if(flags & F_IPC_NONBLOCKING)
	{
		/* Make the file descriptor non-blocking so future operations do
		 * not block. */
		ret = fcntl(ipc.aryIpcChans[chan].sock,
				F_SETFL,
				O_NONBLOCK);
		if(ret < 0)
		{
			OscLog(ERROR, "%s: Unable to make socket non-blocking! (%s)\n",
					__func__,
					strerror(errno));
			return -ESOCKET;
		}
	}
	
	ipc.aryIpcChans[chan].flags = flags;
	strcpy(ipc.aryIpcChans[chan].strSocketPath, strSocketPath);
	
	if(flags & F_IPC_SERVER)
	{
		/* Open the IPC channel as a server. This means just binding
		 * to the specified socket name and waiting for incoming
		 * requests. */
		
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, ipc.aryIpcChans[chan].strSocketPath);
		len = strlen(addr.sun_path) + sizeof(addr.sun_family);
		
		/* Remove the socket file before bind() if already present */
		unlink(addr.sun_path);
		
		ret = bind(ipc.aryIpcChans[chan].sock,
				(struct sockaddr *)&addr,
				len);
		if(ret < 0)
		{
			OscLog(ERROR, "%s: Unable to bind socket! (%s)\n",
					__func__,
					strerror(errno));
			close(ipc.aryIpcChans[chan].sock);
			return -ESOCKET;
		}
		
		/* Make the socket file node accessible by all processes. */
		ret = chmod(addr.sun_path, SERV_SOCKET_PERMISSIONS);
		if(ret < 0)
		{
			OscLog(WARN, "%s: Unable to set access permissions of "
					"socket file node \"%s\"! (%s)",
					__func__,
					addr.sun_path,
					strerror(errno));
		}
		
		/* Start listening on the socket. */
		ret = listen(ipc.aryIpcChans[chan].sock, ACCEPT_WAIT_QUEUE_LEN);
		if(ret < 0)
		{
			OscLog(ERROR, "%s: Unable to listen to socket! (%s)\n",
					__func__,
					strerror(errno));
			close(ipc.aryIpcChans[chan].sock);
			unlink(ipc.aryIpcChans[chan].strSocketPath);
			return -ESOCKET;
		}
	} else {
		/* Open the IPC channel as client. Connect to the specified
		 * server socket. */
		
			
		/* Connect to remote socket. */
		addr.sun_family = AF_UNIX;
		strcpy(addr.sun_path, strSocketPath);
		len = strlen(addr.sun_path) + sizeof(addr.sun_family);
		ret = connect(ipc.aryIpcChans[chan].sock,
				(struct sockaddr *)&addr,
				len);
		if(ret < 0)
		{
			if(errno == EINPROGRESS)
			{
				/* Nonblocking connect. */
				OscLog(WARN, "%s: Socket not fully connected yet "
						"due to non-blocking operation...",
						__func__);
			} else {
				OscLog(ERROR, "%s: Unable to connect socket! (%s)\n",
						__func__,
						strerror(errno));
				close(ipc.aryIpcChans[chan].sock);
				unlink(ipc.aryIpcChans[chan].strSocketPath);
				return -ESOCKET;
			}
		}
	}
	
	*pIpcChan = chan;
	ipc.arybIpcChansBusy[chan] = TRUE;
	return SUCCESS;
}

OSC_ERR OscIpcUnregisterChannel(OSC_IPC_CHAN_ID chanID)
{
	struct OSC_IPC_CHANNEL  *pChan;
	
	pChan = &ipc.aryIpcChans[chanID];
	
	if(unlikely((chanID >= MAX_NR_IPC_CHANNELS) ||
			(ipc.arybIpcChansBusy[chanID] == FALSE)))
	{
		return -EINVALID_PARAMETER;
	}
	
	if(pChan->sock > 0)
	{
		close(pChan->sock);
	}
	
	/* Delete the file node associated with this channel's socket. */
	if(pChan->flags & F_IPC_SERVER)
	{
		unlink(ipc.aryIpcChans[chanID].strSocketPath);
	}
	ipc.arybIpcChansBusy[chanID] = FALSE;
	return SUCCESS;
}

inline OSC_ERR OscIpcRecvMsg(const OSC_IPC_CHAN_ID chanID,
		struct OSC_IPC_MSG *pMsg)
{
	return OscIpcRecv(chanID, pMsg, sizeof(struct OSC_IPC_MSG));
}

OSC_ERR OscIpcRecv(const OSC_IPC_CHAN_ID chanID,
		void *pData,
		const uint32 dataLen)
{
	int                     ret;
	struct OSC_IPC_CHANNEL  *pChan;
	struct sockaddr_un      remoteAddr;
	socklen_t               remoteAddrLen;
	int                     sock;
	
	/* No input validation since this is only called by module-internal
	 * functions. */
	
	pChan = &ipc.aryIpcChans[chanID];
	
	if(pChan->flags & F_IPC_SERVER)
	{
		if(pChan->acceptedSock <= 0)
		{
			/* No connected accepted yet. */
			remoteAddrLen = sizeof(remoteAddr);
			pChan->acceptedSock = accept(pChan->sock,
											(struct sockaddr*)&remoteAddr,
											&remoteAddrLen);
			if(pChan->acceptedSock < 0)
			{
				if(likely((errno == EAGAIN) || (errno == EWOULDBLOCK)))
				{
					/* No connection request pending.*/
					return -ENO_MSG_AVAIL;
				} else {
					OscLog(ERROR, "%s: Accepting connection failed! (%s)\n",
							__func__, strerror(errno));
					return -ESOCKET;
				}
			}
			
			if(pChan->flags & F_IPC_NONBLOCKING)
			{
				/* Make the file descriptor non-blocking so receive and
				 * send commands do not block. */
				ret = fcntl(pChan->acceptedSock,
						F_SETFL,
						O_NONBLOCK);
				if(ret < 0)
				{
					OscLog(ERROR,
							"%s: Unable to make socket non-blocking! (%s)\n",
							__func__,
							strerror(errno));
					return -ESOCKET;
				}
			}
		}
		sock = pChan->acceptedSock;
	} else {
		sock = pChan->sock;
	}
	
	if(pChan->flags & F_IPC_NONBLOCKING)
	{
		/* If non-blocking, first peek at the data to see if a full
		 * message has arrived. */
		errno = 0;
		ret = recv(sock,
				pData,
				dataLen,
				MSG_PEEK);

		if(ret != dataLen)
		{
			if(likely(errno == EAGAIN || errno == EWOULDBLOCK))
			{
				/* No messages waiting */
				return -ENO_MSG_AVAIL;
			} else if(errno == 0 && ret == 0) { /* EOF */
				/* Remote end of socket shut down. Mark it as invalid, so
				 * the next call will try to accept a new connection. */
				close(pChan->acceptedSock);
				pChan->acceptedSock = -1;
				return -ENO_MSG_AVAIL;
			} else {
				OscLog(ERROR, "%s: Reading pending messages failed! (%s)\n",
						__func__, strerror(errno));
				return -ESOCKET;
			}
		}
	}
	
	errno = 0;
	ret = recv(sock,
			pData,
			dataLen,
			MSG_WAITALL);
	
	if(ret != dataLen)
	{
		if(likely(errno == EAGAIN || errno == EWOULDBLOCK))
		{
			/* No messages waiting */
			return -ENO_MSG_AVAIL;
		} else if(errno == 0 && ret == 0) { /* EOF */
			/* Remote end of socket shut down. Mark it as invalid, so
			 * the next call will try to accept a new connection. */
			close(pChan->acceptedSock);
			pChan->acceptedSock = -1;
			return -ENO_MSG_AVAIL;
		} else {
			OscLog(ERROR, "%s: Reading pending messages failed! (%s)\n",
					__func__, strerror(errno));
			return -ESOCKET;
		}
	}
	return SUCCESS;
}

inline OSC_ERR OscIpcSendMsg(const OSC_IPC_CHAN_ID chanID,
		const struct OSC_IPC_MSG *pMsg)
{
	return OscIpcSend(chanID, (void*)pMsg, sizeof(struct OSC_IPC_MSG));
}

OSC_ERR OscIpcSend(const OSC_IPC_CHAN_ID chanID,
		const void *pData,
		uint32 dataLen)
{
	int                     ret = 0;
	struct OSC_IPC_CHANNEL  *pChan;
	int                     sock;
	
	pChan = &ipc.aryIpcChans[chanID];
	
	if(pChan->flags & F_IPC_SERVER)
	{
		sock = pChan->acceptedSock;
	} else {
		sock = pChan->sock;
	}
	/* No input validation since this is only called by module-internal
	 * functions. */
	while ( dataLen != 0 )
	{
		ret = send(sock, pData, dataLen, 0);
		if (unlikely(ret == -1))
		{
			if ( errno == EAGAIN )
			{
				usleep(5000);
			} else
				break;
		}
		else
		{
			dataLen -= ret;
			pData = (char*)pData + ret;
		}
	}
	
	if(unlikely(ret == -1))
	{

		OscLog(ERROR, "%s: Sending to remote process failed! (%s)\n",
				__func__, strerror(errno));
		return -ESOCKET;
	}
	
	return SUCCESS;
}

