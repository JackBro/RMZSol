#include "Driver.h"
#include "Util.h"
#include "NetBuffer.h"
#include "Irp.h"
#include "ConnectionContext.h"

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

// forward declarations
void NTAPI ClassifyFnConnect(
	const FWPS_INCOMING_VALUES0* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT0* classifyOut);

void NTAPI ClassifyFnStream(
	const FWPS_INCOMING_VALUES0* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT0* classifyOut);

NTSTATUS NTAPI NotifyFn(
	FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	const GUID* filterKey,
	FWPS_FILTER* filter);

void NTAPI FlowDeleteFn(
	UINT16 layerId,
	UINT32 calloutId,
	UINT64 flowContext);

void Unload(
	PDRIVER_OBJECT driverObject);

//
// Global variables
//
UINT32 CalloutConnectId;
UINT32 CalloutStreamId;
PDEVICE_OBJECT DeviceObject = NULL;
LPCWSTR wstrDeviceName = L"\\Device\\rmzdrv";
LPCWSTR wstrSymlinkName = L"\\??\\rmzdrv";
UNICODE_STRING deviceName = { 0 };
UNICODE_STRING symlinkName = { 0 };

//
// Entry point for dtiver
//
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driverObject, _In_ PUNICODE_STRING registryPath)
{
    NTSTATUS status;
	FWPS_CALLOUT calloutConnect = { 0 };
	FWPS_CALLOUT calloutStream = { 0 };
	
	/* Specify unload handler */
	driverObject->DriverUnload = DriverUnload;

	/* Dispatchers */
	driverObject->MajorFunction[IRP_MJ_CREATE] = rmzDispatchCreate;
	driverObject->MajorFunction[IRP_MJ_CLOSE] = rmzDispatchClose;
	driverObject->MajorFunction[IRP_MJ_READ] = rmzDispatchRead;
	driverObject->MajorFunction[IRP_MJ_WRITE] = rmzDispatchWrite;

	/* Give a name for our device */
	RtlInitUnicodeString(&deviceName, wstrDeviceName);
	RtlInitUnicodeString(&symlinkName, wstrSymlinkName);

	/* Create i/o device */
	status = IoCreateDevice(driverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceObject);

	if (!CheckStatus(status, "IoCreateDevice") || !DeviceObject) goto exit;

	DeviceObject->Flags |= DO_BUFFERED_IO;

	/* Init connection context */
	RmzInitQueue();

	/* Create symbolic link, to allow open device as file from user space */
	status = IoCreateSymbolicLink(&symlinkName, &deviceName);

	CheckStatus(status, "IoCreateSymbolicLink");

	/* Register connect callout */
	calloutConnect.calloutKey = rmzCalloutConnectGuid;
	calloutConnect.classifyFn = ClassifyFnConnect;
	calloutConnect.notifyFn = NotifyFn;

	status = FwpsCalloutRegister(DeviceObject, &calloutConnect, &CalloutConnectId);

	if (!CheckStatus(status, "FwpsCalloutRegister(calloutConnect)")) goto exit;

	/* Register stream callout */
	calloutStream.calloutKey = rmzCalloutStreamGuid;
	calloutStream.classifyFn = ClassifyFnStream;
	calloutStream.notifyFn = NotifyFn;
	calloutStream.flowDeleteFn = FlowDeleteFn;
	calloutStream.flags = FWP_CALLOUT_FLAG_CONDITIONAL_ON_FLOW;

	status = FwpsCalloutRegister(DeviceObject, &calloutStream, &CalloutStreamId);

	if (!CheckStatus(status, "FwpsCalloutRegister(calloutStream)")) goto exit;

exit:
    return status;

	UNREFERENCED_PARAMETER(registryPath);
}

void DriverUnload(PDRIVER_OBJECT driverObject)
{
	NTSTATUS status;

	// unregister connect callout
	status = FwpsCalloutUnregisterById(CalloutConnectId);
	CheckStatus(status, "FwpsCalloutUnregisterById(CalloutConnectId)");

	// unregister stream callout
	status = FwpsCalloutUnregisterById(CalloutStreamId);

	if (status == STATUS_DEVICE_BUSY)
	{
		DbgPrint("STATUS_DEVICE_BUSY, removing all contexts first\r\n");
		// removing associations force calling flowDeleteFn, there context data actually frees
		// rmzRemoveAllFlowContexts();
		// try to unregister again
		status = FwpsCalloutUnregisterById(CalloutStreamId);
	}

	CheckStatus(status, "FwpsCalloutUnregisterById(CalloutStreamId)");

	status = IoDeleteSymbolicLink(&symlinkName);

	CheckStatus(status, "IoDeleteSymbolicLink");

	IoDeleteDevice(DeviceObject);

	UNREFERENCED_PARAMETER(driverObject);
}

void NTAPI ClassifyFnConnect(
	const FWPS_INCOMING_VALUES0* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT0* classifyOut)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(layerData);
	UNREFERENCED_PARAMETER(classifyContext);
	UNREFERENCED_PARAMETER(flowContext);

	if (inFixedValues->layerId == FWPS_LAYER_ALE_FLOW_ESTABLISHED_V4)
	{
		status = FwpsFlowAssociateContext(inMetaValues->flowHandle, FWPS_LAYER_STREAM_V4, CalloutStreamId, inMetaValues->flowHandle);
		CheckStatus(status, "FwpsFlowAssociateContext");
	}

	if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
	{
		classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	}

	classifyOut->actionType = FWP_ACTION_PERMIT;
}

void NTAPI ClassifyFnStream(
	const FWPS_INCOMING_VALUES0* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT0* classifyOut)
{
	UNREFERENCED_PARAMETER(inMetaValues);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(classifyContext);

	classifyOut->actionType = FWP_ACTION_BLOCK;

	if (layerData == NULL)
		return;

	FWPS_STREAM_CALLOUT_IO_PACKET* packet = layerData;
	FWPS_STREAM_DATA* streamData = packet->streamData;

	if (inFixedValues->layerId == FWPS_LAYER_STREAM_V4)
	{
		if (streamData->flags & FWPS_STREAM_FLAG_RECEIVE_DISCONNECT || streamData->flags & FWPS_STREAM_FLAG_SEND_DISCONNECT)
			classifyOut->actionType = FWP_ACTION_PERMIT;

		RmzQueuePacket(flowContext, streamData);
	}
}

NTSTATUS NotifyFn(
	FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	const GUID* filterKey,
	FWPS_FILTER* filter)
{
	UNREFERENCED_PARAMETER(filterKey);

	switch (notifyType)
	{
	case FWPS_CALLOUT_NOTIFY_ADD_FILTER:
		DbgPrint("Filter added %llu\r\n", filter->filterId);
		break;

	case FWPS_CALLOUT_NOTIFY_DELETE_FILTER:
		DbgPrint("Filter deleted %llu\r\n", filter->filterId);
		break;

	default:
		DbgPrint("Unknown notify type %d\r\n", notifyType);
	}

	return STATUS_SUCCESS;
}

void FlowDeleteFn(
	UINT16 layerId,
	UINT32 calloutId,
	UINT64 flowContext)
{
	// TODO: this funtion can be called from different layers and callouts, so need to count layer and callout ids
	UNREFERENCED_PARAMETER(layerId);
	UNREFERENCED_PARAMETER(calloutId);

	DbgPrint("FlowDeleteFn %llu\r\n", flowContext);

	//rmzFreeFlowContext(flowContext);
}
