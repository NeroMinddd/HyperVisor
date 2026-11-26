#include <ntddk.h>
#include <stdbool.h>
#include <intrin.h>

#define  IA32_FEATURE_CONTROL_MSR 0x3A
#define  IA32_FEATURE_CONTROL_LOCK_BIT (1 << 0)
#define  IA32_FEATURE_CONTROL_VMX_OUTSIDE_SMX (1 << 2)
#define  IA32_VMX_CR0_FIXED0 (0x486)
#define  IA32_VMX_CR0_FIXED1 (0x487) 
#define  IA32_VMX_CR4_FIXED0 (0x488)
#define  IA32_VMX_CR4_FIXED1 (0x489)

#define PAGE_SIZE 4096

typedef struct _VCPU_data {
    PVOID VmxonVa;
    PHYSICAL_ADDRESS VmxonPa;
    PVOID VmcsVa;
    PHYSICAL_ADDRESS VmcsPa;
} Vcpu, *PVCPU_DATA;

PVCPU_DATA VcpuDataArray = NULL;
int proccessorCount = 0;

bool Cpuid_Check()
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    
    bool check_virt = (cpuInfo[2] & (1U << 5)) != 0;
     return check_virt;
    
}

NTSTATUS checkControlRegisters(){
    ULONG_PTR cr0 = __readcr0();
    cr0 |= __readmsr(IA32_VMX_CR0_FIXED0);
    cr0 &= __readmsr(IA32_VMX_CR0_FIXED1);
    __writecr0(cr0);

    ULONG_PTR cr4 = __readcr4();
    cr4 |= __readmsr(IA32_VMX_CR4_FIXED0);
    cr4 &= __readmsr(IA32_VMX_CR4_FIXED1);
    __writecr4(cr4);
    
    return STATUS_SUCCESS;
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

NTSTATUS AllocateMemory(PVCPU_DATA Vcpu){
    PVOID vmxonVa = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, 0, -1, 0, MmCached);

    PVOID vmcsVa = MmAllocateContiguousMemorySpecifyCache(PAGE_SIZE, 0, -1, 0, MmCached);

    if(!vmxonVA || !vmcsVA){
        if(vmxonVa) MmFreeContiguousMemorySpecifyCache(vmxonVa, PAGE_SIZE, MmCached);
        
        if(vmcsVa) MmFreeContiguousMemorySpecifyCache(vmcsVa, PAGE_SIZE, MmCached);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(vmcsVa, PAGE_SIZE);
    RtlZeroMemory(vmxonVa, PAGE_SIZE);

    

    UINT32 revID = (UINT32)(__readmsr(0x480) & (0x7FFFFFFF));

    *(UINT32*)vmxonVa = revID;
    *(UINT32*)vmcsVa = revID;

    Vcpu->VmxonVa = vmxonVa;
    Vcpu->VmxonPa = MmGetPhysicalAddress(VmxonVa);

    Vcpu->VmcsVa = vmcsVa;
    Vcpu->VmcsPa = MmGetPhysicalAddress(vmcsVa);

    return STATUS_SUCCESS;

}

void free_memory(PVCPU_DATA Vcpu){
    if(Vcpu->VmxonVa){
        MmFreeContiguousMemorySpecifyCache(Vcpu->VmxonVa, PAGE_SIZE, MmCached);
        Vcpu->VmxonPa.QuadPart = 0LL;
        Vcpu->VmxonVa = NULL;
    }

    if(Vcpu->VmcsVa){
        MmFreeContiguousMemorySpecifyCache(Vcpu->VmcsVa, PAGE_SIZE, MmCached);
        Vcpu->VmcsPa.QuadPart = 0LL;
        Vcpu->VmcsVa = NULL;
    }
}

NTSTATUS sincForEveryCore(void) {
    NTSTATUS status = STATUS_SUCCESS;
    proccessorCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);

    VcpuDataArray = (PVCPU_DATA)ExAllocatePool2(
        POOL_FLAG_NON_PAGED 
         POOL_FLAG_ZERO_ALLOCATION,
        (SIZE_T)proccessorCount * sizeof(Vcpu),
        'VCPU'
    );

    if(!VcpuDataArray){
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (int i = 0; i < proccessorCount; i++){
        KAFFINITY Old_Affinity = KeSetSystemAffinityThreadEx(1ULL << i);
        
        status = AllocateMemory(&VcpuDataArray[i]);

        NTSTATUS CheckCtrl = checkControlRegisters();

        ULONG CpuID = Cpuid_Check();

        ULONG IA32 = IA32_FEATURE_CONTROL();

        if(!NT_STATUS(status) || !NT_STATUS(CheckCtrl) || !CpuID || !IA32){
            for(int k = 0; k < proccessorCount; k++){
                free_memory(&VcpuDataArray[k]);
            }
            ExFreePoolWithTag(VcpuDataArray, 'VCPU');
            VcpuDataArray = NULL;
            proccessorCount = 0;
            KeRevertToUserAffinityThreadEx(Old_Affinity);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        KeRevertToUserAffinityThreadEx(Old_Affinity);
        
    }

    return STATUS_SUCCESS;

}


void DriverUnload(PDRIVER_OBJECT DriverObject){
    UNREFERENCED_PARAMETER(DriverObject);

    if (proccessorCount) {
        for (int i = 0; i < proccessorCount; i++) {
            free_memory(&VcpuDataArray[i]);
        }
        ExFreePoolWithTag(VcpuDataArray, 'VCPU');
        proccessorCount = 0;
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);


    DriverObject->DriverUnload = DriverUnload;

    if(!Cpuid_Check() || !IA32_FEATURE_CONTROL()){
        DbgPrint("VMX is not supported or disapled in BIOS. \n");

        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS status = sincForEveryCore();
    if(!NT_SUCCESS(status)){
        DbgPrint("Failed to allocate memory. \n");
        return status;
    }

    DbgPrint("Memory is successfully allocated. \n");
    
    return STATUS_SUCCESS;
}