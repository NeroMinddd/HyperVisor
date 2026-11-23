#include <ntddk.h>
#include <stdbool.h>
#include <intrin.h>

#define  IA32_FEATURE_CONTROL_MSR 0x3A
#define  IA32_FEATURE_CONTROL_LOCK_BIT (1 << 0)
#define  IA32_FEATURE_CONTROL_VMX_OUTSIDE_SMX (1 << 2)

#define PAGE_SIZE 4096

PVCPU_DATA VcpuDataArray = NULL;
ULONG proccessorCount = 0;


typedef struct _VCPU_data {
    PVOID VmxonVa;
    PHYSICAL_ADDRESS VmxonPa;
    PVOID VmcsVa;
    PHYSICAL_ADDRESS VmcsPa;
} Vcpu, *PVCPU_DATA;


bool Cpuid_Check()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    
    bool check_virt = (cpuInfo[2] & (1U << 5)) != 0;
     return check_virt;
    
}

bool IA32_FEATURE_CONTROL(){
    ULONG64 msr = __readmsr(IA32_FEATURE_CONTROL_MSR);
    ULONG64 lock_bit = (msr & (IA32_FEATURE_CONTROL_LOCK_BIT));
    ULONG64 vmx_outside_smx = (msr & (IA32_FEATURE_CONTROL_VMX_OUTSIDE_SMX));

   if((msr & IA32_FEATURE_CONTROL_LOCK_BIT)  == 0){
    msr |= (IA32_FEATURE_CONTROL_LOCK_BIT | IA32_FEATURE_CONTROL_VMX_OUTSIDE_SMX);
    __writemsr(IA32_FEATURE_CONTROL_MSR, msr);
    return true;
   }else{
    if(vmx_outside_smx != 0){
        return true;
    }else{
        return false;
    }
   }
}

int AllocateMemory(PVCPU_DATA Vcpu){
    PVOID vmxonVA = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, 0, -1, 0, MmCached);

    PVOID vmcsVA = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, 0, -1, 0, MmCached);

    if(!vmxonVA || !vmcsVA){
        if(vmxonVA) MmFreeContiguousMemorySpecifyCache(vmxonVA, PAGE_SIZE, MmCached);
        
        if(vmcsVA) MmFreeContiguousMemorySpecifyCache(vmcs, PAGE_SIZE, MmCached);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(vmcsVa, PAGE_SIZE);
    RtlZeroMemory(vmxonVa, PAGE_SIZE);

    

    UINT32 revID = (UINT32)(__readmsr(0x480) & (0x7FFFFFFF))

    *(UINT32*)vmxonVA = revID;
    *(UINT32*)vmcsVA = revID;

    VCpu->VmxonVa = vmxonVA;
    VCpu->VmxonPa = MmGetPhysicalAddress(VmxonVa);

    VCpu->VmcsVa = vmcsVA;
    VCpu->VmcsPa = MmGetPhysicalAddress(vmcsVa);

    return STATUS_SUCCESS;

}

int free_memory(PVCPU_DATA){
    if(Vcpu->vmxonVa){
        MmFreeContiguousMemorySpecifyCache(Vcpu->vmxonVa, PAGE_SIZE, MmCached);
        Vcpu->vmxonPa = nullptr;
        Vcpu->vmxonVa = nullptr;
    }

    if(Vcpu->VmcsVa){
        MmFreeContiguousMemorySpecifyCache(Vcpu->vmcxVa, PAGE_SIZE, MmCached);
        Vcpu->vmcsPa = nullptr;
        Vcpu->vmcsVa = nullptr;
    }
}

int sincForEveryCore(VCPU_DATA) {
    ULONG proccessorCount =  KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);

    PVCPU_DATA vcpudDataArray = (PVCPU_DATA) ExAllocatePool(
        POOL_FLAG_NON_PAGED,
        proccessorCount * sizeof(VCPU_DATA),
        'VCPU'
    );

    if(!VcpuDataArray){
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (int i = 0; i < proccessorCount; i++){
        NTSTATUS success = AllocateMemory(&proccessorCount[i]);
        if(!NT_SUCCESS(success)){
            for (int i = 0; i < proccessorCount; i++){
                free_memory(proccessorCount[i]);
            }
            ExFreePoolWithTag(proccessorCount, 'VCPU');
            proccessorCount = NULL;
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

}

void DriverUnload(DriverObject){
    UNREFFRENCED_PARAMETER(DriverObject);

    if (g_VcpuDataArray) {
        for (ULONG i = 0; i < g_ProcessorCount; i++) {
            FreeVcpuMemory(&g_VcpuDataArray[i]);
        }
        ExFreePoolWithTag(g_VcpuDataArray, 'VCPU');
        g_VcpuDataArray = NULL;
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_SUCCESS;

    DriverObject->DriverUnload = DriverUnload;

    if(!Cpuid_Check() || !IA32_FEATURE_CONTROL()){
        DbgPring("VMX is not supported or disapled in BIOS. \n");

        return STATUS_NON_SUPPORTED;
    }

    NTSTATUS status = sincForEveryCore();
    if(!NT_STATUS(status)){
        DbgPrint("Failed to allocate memory. \n");
        return status;
    }

    DbgPrint("Memory is successfully allocated. \n");
    
    return STATUS_SUCCESS;
}