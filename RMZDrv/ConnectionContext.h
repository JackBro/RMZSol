#pragma once

#define NDIS_SUPPORT_NDIS6 1

#include <basetsd.h>

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union
#include <fwpsk.h>
#pragma warning(pop)

typedef struct _FLOW
{
	LIST_ENTRY list;
	UINT64 flowId;
	UINT32 calloutId;
} FLOW, *PFLOW;

typedef struct _FLOW_LIST
{
	LIST_ENTRY flows;
	KSPIN_LOCK lock;
} FLOW_LIST, *PFLOW_LIST;

typedef enum _SOURCE
{
	NEWCONNECTION,
	FROMSERVER,
	FROMCLIENT
} SOURCE;

typedef struct _PACKET
{
	LIST_ENTRY list;
	UINT64 flowId;
	UINT64 serial;
	SOURCE source;
	FWPS_STREAM_DATA* stream;
} PACKET, *PPACKET;

typedef struct _PACKET_QUEUE
{
	LIST_ENTRY packets;
	KSPIN_LOCK lock;
	KEVENT event;
} PACKET_QUEUE, *PPACKET_QUEUE;

void RmzInitQueue();
void RmzQueuePacket(UINT64 flowId, FWPS_STREAM_DATA* stream);
void RmzFreePacket(PPACKET packet);
PPACKET RmzPopPacket();
void RmzFreeQueue();

BOOL RmzWaitOnQueue();
void RmzNotifyQueueNotEmpty();

// Initializes global variable for flows list
void RmzInitFlowList();

// Only deassociate all flows
void RmzDeassociateFlowList();

// Associate and add flow to list
PFLOW RmzAddFlow(UINT64 flowId, UINT32 calloutId);

// Remove flow from list and free allocated memory
void RmzRemoveFlow(PFLOW flow);