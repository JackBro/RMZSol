#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

// forward declarations
void NTAPI ClassifyFn(
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

BOOL CheckStatus(
	NTSTATUS status,
	PCSTR message);

//
// Global variables
//
UINT32 CalloutId;
PDEVICE_OBJECT DeviceObject = NULL;

//
// Entry point for dtiver
//
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driverObject, _In_ PUNICODE_STRING registryPath)
{
    NTSTATUS status;
	FWPS_CALLOUT calloutData = { 0 };

	/* Specify unload handler */
	driverObject->DriverUnload = Unload;

	/* Create i/o device */
	status = IoCreateDevice(driverObject, 0, NULL, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceObject);

	if (!CheckStatus(status, "IoCreateDevice")) goto exit;

	/* Register callout */
	calloutData.calloutKey = rmzCalloutGuid;
	calloutData.classifyFn = ClassifyFn;
	calloutData.notifyFn = NotifyFn;
	calloutData.flowDeleteFn = FlowDeleteFn;

	status = FwpsCalloutRegister(DeviceObject, &calloutData, &CalloutId);

	if (!CheckStatus(status, "FwpsCalloutRegister")) goto exit;

exit:
    return status;

	UNREFERENCED_PARAMETER(registryPath);
}

void Unload(PDRIVER_OBJECT driverObject)
{
	NTSTATUS status;

	status = FwpsCalloutUnregisterById(CalloutId);

	if (status == STATUS_DEVICE_BUSY)
	{
	}
	else 
		CheckStatus(status, "FwpsCalloutUnregisterById");

	IoDeleteDevice(DeviceObject);

	UNREFERENCED_PARAMETER(driverObject);
}

BOOL CheckStatus(NTSTATUS status, PCSTR message)
{
	if (!NT_SUCCESS(status))
	{
		DbgPrint( "%s failed: 0x%X\r\n", message, status);
		return FALSE;
	}
	else
	{
		DbgPrint("%s success\r\n", message);
		return TRUE;
	}
}

void NTAPI ClassifyFn(
	const FWPS_INCOMING_VALUES0* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT0* classifyOut)
{
	UNREFERENCED_PARAMETER(inFixedValues);
	UNREFERENCED_PARAMETER(inMetaValues);
	UNREFERENCED_PARAMETER(layerData);
	UNREFERENCED_PARAMETER(classifyContext);
	UNREFERENCED_PARAMETER(filter);
	UNREFERENCED_PARAMETER(flowContext);

	DbgPrint("I am in callout\r\n");

	classifyOut->actionType = FWP_ACTION_CONTINUE;
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
		DbgPrint("Filter added %d\r\n", filter->filterId);
		break;

	case FWPS_CALLOUT_NOTIFY_DELETE_FILTER:
		DbgPrint("Filter deleted %d\r\n", filter->filterId);
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
	UNREFERENCED_PARAMETER(layerId);
	UNREFERENCED_PARAMETER(calloutId);
	UNREFERENCED_PARAMETER(flowContext);
}
