/*************************************************
 *	Filename: receive.c
 *	Written by: James Johns, Silvestrs Timofejevs
 *	Date: 28/3/2012
 *************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "main.h"
#include "packet.h"
#include "queue.h"
#include "platform.h"

struct lanPacket_s *readPacket(int comPort);
void processPacket(struct lanPacket_s *packet, struct threadData_s *threadData);


/*	Function name: receiveStart
 *	Written by: James Johns, Silvestrs Timofojevs
 *	Parameters:
 *		data - void pointer to a threadData_s structure. stated as void * to maintain compatibility 
 *				with threading libraries.
 *
 *	Notes:
 *		Loops constantly until the programState member of data becomes EXIT.
 *		each loop reads a lanPacket_s structure from the comPort file descriptor member of data,
 *		and processes it via processPacket function.
 *		
 *		
 */
THREAD_RET receiveStart(void *data) {
	struct threadData_s *threadData = (struct threadData_s *) data;
	struct lanPacket_s *Packet;
		
	/* loop forever, reading packets and processing them */
	while (threadData->programState != EXIT) {
		Packet = readPacket(threadData->comPort);
		/* if we have received a valid packet, process it. 
		 * otherwise pause for a short time to wait for more data and free up the CPU */
		if (Packet != NULL)
			processPacket(Packet, threadData);
		else
			waitMilliSecs(20);
	}
	return (THREAD_RET)NULL;
}

/*	Function name: processPacket
 *	Written by: James Johns, Silvestrs Timofojevs
 *	Date: 28/3/2012
 *	Parameters:
 *		packet - a packet structure to be processed.
 *		threadData - The data, shared between all threads, that is required for communication 
 *						between threads and view the programs information.
 *
 *	Notes:
 *		If the packet is for the current user, it is passed to the main thread through the
 *		receiveQueue member of data. If it is not for the current user, it is passed to the
 *		transmitter thread through transmitQueue member of data. If the main thread would not need
 *		to know about the packet, or any other packets are required to be transmitted in response,
 *		it is taken care of here.
 *
 */
void processPacket(struct lanPacket_s *packet, struct threadData_s *threadData) {
	char userID = getCurrentID(threadData->userTable);
	
	/* print debug information if in debug mode */
	if (threadData->debugEnable != 0) {
		wprintw(threadData->messageWindow, "Received packet: {%c%c%c%.10s%.1c}\n", packet->source, 
				packet->destination, packet->packetType, packet->payload, packet->checksum);
		wrefresh(threadData->messageWindow);
	}

	/* check the checksum of the packet. if it is wrong, delete the packet and send a 
	 * NAK_PACKET to the sender */
	if (packet->checksum != packetChecksum(packet)) {
	  /* create a NAK packet to send to packet->source */
/* must be proxied to seem like it was only interchanged between the true source and destinations */
	  packet->packetType = NAK_PACKET;
	  userID = packet->source;
	  packet->source = packet->destination;
	  packet->destination = userID;
	  packet->checksum = packetChecksum(packet);
	  addToQueue(threadData->transmitQueue, packet);
	  return;
	}
	/* if the packet is meant for the user currently logged in, the packet needs to be checked */
	if (packet->destination == userID) {
		/*-----------------------------------------------------------------*/
		/* make sure packet is valid via checksum */
		if (packet->packetType == LOGIN_PACKET) {
			/* if we are waiting for a login packet, and we get one, place it on the receive queue
				otherwise if the packet came from our user ID, transmit ACK PACKET
				if none of the above, we have a false packet and need to deal with it. */
			removePendingPacketFromQueue(threadData->transmitQueue, packet);
			if (threadData->programState == LOGIN_PEND)
				addToQueue(threadData->receiveQueue, packet);
			else {
			  packet->packetType = NAK_PACKET;
			  packet->checksum = packetChecksum(packet);
			  addToQueue(threadData->transmitQueue,packet);
			}
		}
		else if (packet->packetType == DATA_PACKET) {
			struct lanPacket_s *ackPacket = createLanPacket(packet->destination, packet->source, 
															ACK_PACKET, packet->payload);
			addToQueue(threadData->transmitQueue, ackPacket);
			wprintw(threadData->messageWindow, "Message from %c: %.10s\n", packet->source, 
					packet->payload);
			wrefresh(threadData->messageWindow);
			destroyPacket(packet);
		}
		else if (packet->packetType == RESPONSE_PACKET) {
			addToUserTable(threadData->userTable, packet->source);
			wprintw(threadData->messageWindow, "found user on network\n");
			wrefresh(threadData->messageWindow);
			destroyPacket(packet);
		}
		else if (packet->packetType == ACK_PACKET) {
			/* remove packet from lan, and removce pending packet from transmit queue */
			removePendingPacketFromQueue(threadData->transmitQueue, packet);
			destroyPacket(packet);
		}
		else if (packet->packetType == NAK_PACKET) {
		  /* find the packet in transmit queue and mark it for transmission ASAP */
			/* find where the packet is in the queue, lock the queue, then move the front of the 
			 * queue to the position of the packet in the queue to expedited in transmission */
			struct queue_s *queue = findQueueItemRelativeToPacket(threadData->transmitQueue, packet);
			if (threadData->programState == LOGIN_PEND) {
			  threadData->programState = LOGIN;
			  setCurrentID(threadData->userTable, 0);
			}
			else if (queue != NULL) {
				/* expedite queue */
				expediteQueueItemToFront(threadData->transmitQueue, queue);
			}
		}
		else if (packet->packetType == LOGOUT_PACKET) {
		  if (threadData->programState != LOGOUT) {
			/* we're not trying to logout, so stop anyone from forcing us offline */
		    destroyPacket(packet);
		  }
		  else {
			removePendingPacketFromQueue(threadData->transmitQueue, packet);
		    addToQueue(threadData->receiveQueue, packet);
		  }
		}
		/* don't accept packets if we're logging out, only report back with our own logout packet */
		else {
			addToQueue(threadData->receiveQueue, packet);
		}
	}
	else {
		/* the packet is not for the current user, so pass it on to the next station, unless it's a 
		 * LOGIN_PACKET or LOGOUT_PACKET */
		if (packet->source == userID) {
			/* packet has returned to us, destroy it */
			wprintw(threadData->messageWindow, "Failed message send, retrying up to 5 times\n");
			wrefresh(threadData->messageWindow);
			destroyPacket(packet);
		}
		else {
			if (packet->packetType == LOGIN_PACKET) {
				/* new user appears, add entry to user table and update the display */
				addToUserTable(threadData->userTable, packet->source);
				printUserTable(threadData->userTable, threadData->userListWindow);
				/* if there is a user currently logged in, transmit a response packet to let new 
				 * user know the current user is logged in */
				if (userID != 0) {
					struct lanPacket_s *respPacket = createLanPacket(userID, packet->source, RESPONSE_PACKET, NULL);
					respPacket->pending = 1; /* stop transmitter from pending the packet for 5
											  * tries by setting last transmit to non-zero */
					respPacket->lastTransmit = 1;
					respPacket->checksum = packetChecksum(respPacket);
					addToQueue (threadData->transmitQueue, respPacket);
				}
			}
			else if (packet->packetType == LOGOUT_PACKET) {
				removeFromUserTable(threadData->userTable, packet->source);
				printUserTable(threadData->userTable, threadData->userListWindow);
			}
			/* add packet to transmit queue for sending to next station */
			addToQueue (threadData->transmitQueue, packet);
		}
	}
}

/*	Function name: readPacket
 *	Written by: James Johns, Silvestrs Timofojevs
 *	Date: 28/3/2012
 *	Parameters:
 *		comPort - A file descriptor for the COM port being used for the LAN.
 *	Retuns:
 *		A lanPacket_s structure, created by the createLanPacket function. MUST be destroyed 
 *		by destroyPacket.
 *
 *	notes:
 *		Blocks the calling thread until a full packet has been read.
 *		return value must be freed by the caller.
 *
 */
struct lanPacket_s *readPacket(int comPort) {
	int i; 
	char Source, Destination, tmp;
	struct lanPacket_s *Packet = NULL;
	enum PacketType packetType;
	/* try reading from the com port, if there is the beginning of a packet, read in the rest 
	 * of the packet, otherwise pause execution for a short time to let the CPU do other things,
	 * and wait for more data to become available on the COM port */
	if (read(comPort, &tmp, 1) != 0 && (tmp == PACKET_START) ) {
		/* Async compatible blocking reads.  */
		while (read(comPort, &Destination, 1) != 1)
			waitMilliSecs(20);
		while (read(comPort, &Source, 1) != 1)
			waitMilliSecs(20);
		while (read(comPort, &tmp, 1) != 1)
			waitMilliSecs(20);
		packetType = (enum PacketType)tmp;
		Packet = createLanPacket(Source, Destination, packetType, NULL);
		i = 0;
		/* read a maximum of 10 bytes over several loops. i keeps count of bytes read so far. */
		while (i < 10) {
			i += read(comPort, (Packet->payload)+i, 10-i);
			/* not enough bytes read, pause and wait for more bytes to become available */
			if (i < 10) 
				waitMilliSecs(20);
		}
		while (read(comPort, &Packet->checksum, 1) != 1)
			waitMilliSecs(20);
		/* make sure we read to the end of the packet, to synchronise packet reads/writes between 
		 * stations */
		while (1) {
			if (read(comPort, &tmp, 1) == 1 && tmp == PACKET_END) {
				break;
			}
			else {
				waitMilliSecs(20);
				continue;
			}
		}
	}
	else {
		waitMilliSecs(20);
	}
	return Packet;
}
