#include <furry_hal_mpu.h>
#include <stm32wbxx_ll_cortex.h>

#define FURRY_HAL_MPU_ATTRIBUTES                                                     \
    (LL_MPU_ACCESS_BUFFERABLE | LL_MPU_ACCESS_CACHEABLE | LL_MPU_ACCESS_SHAREABLE | \
     LL_MPU_TEX_LEVEL1 | LL_MPU_INSTRUCTION_ACCESS_ENABLE)

#define FURRY_HAL_MPU_STACK_PROTECT_REGION FurryHalMPURegionSize32B

void furry_hal_mpu_init() {
    furry_hal_mpu_enable();

    // NULL pointer dereference protection
    furry_hal_mpu_protect_no_access(FurryHalMpuRegionNULL, 0x00, FurryHalMPURegionSize1MB);
}

void furry_hal_mpu_enable() {
    LL_MPU_Enable(LL_MPU_CTRL_PRIVILEGED_DEFAULT);
}

void furry_hal_mpu_disable() {
    LL_MPU_Disable();
}

void furry_hal_mpu_protect_no_access(
    FurryHalMpuRegion region,
    uint32_t address,
    FurryHalMPURegionSize size) {
    uint32_t size_ll = size;
    size_ll = size_ll << MPU_RASR_SIZE_Pos;

    furry_hal_mpu_disable();
    LL_MPU_ConfigRegion(
        region, 0x00, address, FURRY_HAL_MPU_ATTRIBUTES | LL_MPU_REGION_NO_ACCESS | size_ll);
    furry_hal_mpu_enable();
}

void furry_hal_mpu_protect_read_only(
    FurryHalMpuRegion region,
    uint32_t address,
    FurryHalMPURegionSize size) {
    uint32_t size_ll = size;
    size_ll = size_ll << MPU_RASR_SIZE_Pos;

    furry_hal_mpu_disable();
    LL_MPU_ConfigRegion(
        region, 0x00, address, FURRY_HAL_MPU_ATTRIBUTES | LL_MPU_REGION_PRIV_RO_URO | size_ll);
    furry_hal_mpu_enable();
}

void furry_hal_mpu_protect_disable(FurryHalMpuRegion region) {
    furry_hal_mpu_disable();
    LL_MPU_DisableRegion(region);
    furry_hal_mpu_enable();
}

void furry_hal_mpu_set_stack_protection(uint32_t* stack) {
    // Protection area address must be aligned to region size
    uint32_t stack_ptr = (uint32_t)stack;
    uint32_t mask = ((1 << (FURRY_HAL_MPU_STACK_PROTECT_REGION + 2)) - 1);
    stack_ptr &= ~mask;
    if(stack_ptr < (uint32_t)stack) stack_ptr += (mask + 1);

    furry_hal_mpu_protect_read_only(
        FurryHalMpuRegionStack, stack_ptr, FURRY_HAL_MPU_STACK_PROTECT_REGION);
}