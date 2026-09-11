#include "tests/test_framework.hpp"
#include "xblob/common/error.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/mmio.hpp"
#include "xblob/memory/permission.hpp"
#include "xblob/memory/ram.hpp"

using namespace xblob;
using namespace xblob::memory;

// Task 2.1: RAM física configurável 64/128 MiB, zero inicial, capacidade inválida, último byte
TEST_CASE(TestRamCreationAndCapacity) {
    // 64 MiB retail
    auto ram64_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram64_res.has_value());
    EXPECT_EQ(ram64_res->size(), kRamSizeRetail);
    // Verificar que os primeiros bytes e o último byte são zero
    EXPECT_EQ(ram64_res->Read8(0).value(), 0);
    EXPECT_EQ(ram64_res->Read8(kRamSizeRetail - 1).value(), 0);

    // Escrever e ler no último byte
    EXPECT_TRUE(ram64_res->Write8(kRamSizeRetail - 1, 0xAB).has_value());
    EXPECT_EQ(ram64_res->Read8(kRamSizeRetail - 1).value(), 0xAB);

    // Fora dos limites
    EXPECT_FALSE(ram64_res->Read8(kRamSizeRetail).has_value());
    EXPECT_FALSE(ram64_res->Write8(kRamSizeRetail, 0x12).has_value());

    // 128 MiB devkit
    auto ram128_res = Ram::Create(kRamSizeDevkit);
    EXPECT_TRUE(ram128_res.has_value());
    EXPECT_EQ(ram128_res->size(), kRamSizeDevkit);
    EXPECT_EQ(ram128_res->Read8(kRamSizeDevkit - 1).value(), 0);

    // Capacidade inválida
    auto invalid_ram1 = Ram::Create(32 * 1024 * 1024);
    EXPECT_FALSE(invalid_ram1.has_value());
    EXPECT_EQ(invalid_ram1.error().code, ErrorCode::InvalidCapacity);

    auto invalid_ram2 = Ram::Create(0);
    EXPECT_FALSE(invalid_ram2.has_value());
    EXPECT_EQ(invalid_ram2.error().code, ErrorCode::InvalidCapacity);

    auto invalid_ram3 = Ram::Create(kRamSizeRetail + 1024);
    EXPECT_FALSE(invalid_ram3.has_value());
    EXPECT_EQ(invalid_ram3.error().code, ErrorCode::InvalidCapacity);
}

// Task 2.2: Regiões e espaço de endereçamento de 32 bits, adjacência, sobreposição atômica,
// unmapped, overflow
TEST_CASE(TestAddressSpaceMappingAndOverlap) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace as;

    // Mapeamento 1: [0x1000, 0x2000) (4096 bytes)
    auto map1 = as.MapRam(0x1000, 0x1000, ram, 0, MemoryPermission::All);
    EXPECT_TRUE(map1.has_value());
    EXPECT_EQ(as.region_count(), 1ULL);

    // Mapeamento adjacente: [0x2000, 0x3000) deve funcionar
    auto map2 = as.MapRam(0x2000, 0x1000, ram, 0x1000, MemoryPermission::All);
    EXPECT_TRUE(map2.has_value());
    EXPECT_EQ(as.region_count(), 2ULL);

    // Mapeamento sobreposto parcial início: [0x0800, 0x1800) deve falhar atomicamente
    auto map_overlap1 = as.MapRam(0x0800, 0x1000, ram, 0x2000, MemoryPermission::All);
    EXPECT_FALSE(map_overlap1.has_value());
    EXPECT_EQ(map_overlap1.error().code, ErrorCode::RegionOverlap);
    EXPECT_EQ(as.region_count(), 2ULL); // Inalterado

    // Mapeamento sobreposto parcial fim: [0x2800, 0x3800) deve falhar atomicamente
    auto map_overlap2 = as.MapRam(0x2800, 0x1000, ram, 0x2000, MemoryPermission::All);
    EXPECT_FALSE(map_overlap2.has_value());
    EXPECT_EQ(map_overlap2.error().code, ErrorCode::RegionOverlap);
    EXPECT_EQ(as.region_count(), 2ULL); // Inalterado

    // Mapeamento idêntico: [0x1000, 0x2000)
    auto map_overlap3 = as.MapRam(0x1000, 0x1000, ram, 0x2000, MemoryPermission::All);
    EXPECT_FALSE(map_overlap3.has_value());
    EXPECT_EQ(map_overlap3.error().code, ErrorCode::RegionOverlap);
    EXPECT_EQ(as.region_count(), 2ULL);

    // Unmapped access
    EXPECT_EQ(as.Read8(0x0000).error().code, ErrorCode::UnmappedAddress);
    EXPECT_EQ(as.Read8(0x0FFF).error().code, ErrorCode::UnmappedAddress);
    EXPECT_EQ(as.Read8(0x3000).error().code, ErrorCode::UnmappedAddress);

    // Overflow no fim de uint32_t: base 0xFFFFFFFF com size 2
    auto map_overflow = as.MapRam(0xFFFFFFFF, 2, ram, 0, MemoryPermission::All);
    EXPECT_FALSE(map_overflow.has_value());
    EXPECT_EQ(map_overflow.error().code, ErrorCode::OutOfBounds);
}

// Task 2.3: Leitura e escrita de 8/16/32 bits little-endian, desalinhamento, cruzamento de limite,
// sem escrita parcial
TEST_CASE(TestLittleEndianAndUnalignedAccess) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace as;
    EXPECT_TRUE(as.MapRam(0x1000, 0x100, ram, 0, MemoryPermission::All).has_value());

    // Round-trip 32 bits: 0x12345678 -> bytes 78 56 34 12
    EXPECT_TRUE(as.Write32(0x1000, 0x12345678).has_value());
    EXPECT_EQ(as.Read8(0x1000).value(), 0x78);
    EXPECT_EQ(as.Read8(0x1001).value(), 0x56);
    EXPECT_EQ(as.Read8(0x1002).value(), 0x34);
    EXPECT_EQ(as.Read8(0x1003).value(), 0x12);
    EXPECT_EQ(as.Read32(0x1000).value(), 0x12345678U);

    // Round-trip 16 bits
    EXPECT_TRUE(as.Write16(0x1010, 0xBEEF).has_value());
    EXPECT_EQ(as.Read8(0x1010).value(), 0xEF);
    EXPECT_EQ(as.Read8(0x1011).value(), 0xBE);
    EXPECT_EQ(as.Read16(0x1010).value(), 0xBEEFU);

    // Acesso desalinhado (permitido dentro da mesma região)
    EXPECT_TRUE(as.Write32(0x1001, 0xCAFEBABE).has_value());
    EXPECT_EQ(as.Read32(0x1001).value(), 0xCAFEBABE);

    // Acesso que cruza o fim da região: região termina em 0x1100 (base 0x1000 + size 0x100)
    // Escrita em 0x10FE com 4 bytes precisa ir até 0x1102 (cruza o limite!)
    // Deve falhar antes de modificar qualquer byte
    u8 orig1 = as.Read8(0x10FE).value();
    u8 orig2 = as.Read8(0x10FF).value();

    auto cross_write = as.Write32(0x10FE, 0xDEADBEEF);
    EXPECT_FALSE(cross_write.has_value());
    EXPECT_EQ(cross_write.error().code, ErrorCode::OutOfBounds);

    // Bytes originais devem estar intactos (sem escrita parcial)
    EXPECT_EQ(as.Read8(0x10FE).value(), orig1);
    EXPECT_EQ(as.Read8(0x10FF).value(), orig2);

    // Leitura que cruza limite também falha
    auto cross_read = as.Read32(0x10FE);
    EXPECT_FALSE(cross_read.has_value());
    EXPECT_EQ(cross_read.error().code, ErrorCode::OutOfBounds);
}

// Task 2.4: Permissões Read / Write / Execute e Fetch
TEST_CASE(TestMemoryPermissionsAndInstructionFetch) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();

    AddressSpace as;
    // Região 1: Read-Only [0x1000, 0x2000)
    EXPECT_TRUE(as.MapRam(0x1000, 0x1000, ram, 0, MemoryPermission::Read).has_value());
    // Região 2: Write-Only [0x2000, 0x3000)
    EXPECT_TRUE(as.MapRam(0x2000, 0x1000, ram, 0x1000, MemoryPermission::Write).has_value());
    // Região 3: Read+Execute (Code) [0x3000, 0x4000)
    EXPECT_TRUE(as.MapRam(0x3000, 0x1000, ram, 0x2000, MemoryPermission::ReadExecute).has_value());

    // Violação de escrita em Read-Only
    auto write_ro = as.Write8(0x1000, 0x55);
    EXPECT_FALSE(write_ro.has_value());
    EXPECT_EQ(write_ro.error().code, ErrorCode::AccessViolation);
    EXPECT_EQ(as.Read8(0x1000).value(), 0); // Intacto

    // Violação de leitura em Write-Only
    auto read_wo = as.Read8(0x2000);
    EXPECT_FALSE(read_wo.has_value());
    EXPECT_EQ(read_wo.error().code, ErrorCode::AccessViolation);

    // Violação de fetch em região sem permissão Execute (Read-Only)
    auto fetch_no_exec = as.Fetch8(0x1000);
    EXPECT_FALSE(fetch_no_exec.has_value());
    EXPECT_EQ(fetch_no_exec.error().code, ErrorCode::AccessViolation);

    // Fetch em região Read+Execute: deve ter sucesso
    EXPECT_TRUE(ram.Write8(0x2000, 0x90).has_value()); // NOP no offset da RAM
    auto fetch_exec = as.Fetch8(0x3000);
    EXPECT_TRUE(fetch_exec.has_value());
    EXPECT_EQ(fetch_exec.value(), 0x90);

    // FetchBytes
    u8 buf[3] = {0x00, 0x00, 0x00};
    EXPECT_TRUE(as.FetchBytes(0x3000, buf).has_value());
    EXPECT_EQ(buf[0], 0x90);
}

// Task 2.5: Backend MMIO tipado com offset relativo, largura, contagem e propagação de Result
TEST_CASE(TestMmioBackend) {
    AddressSpace as;

    u32 last_offset = 0;
    AccessWidth last_width = AccessWidth::Byte;
    u32 last_val = 0;
    int read_count = 0;
    int write_count = 0;

    MmioHandler handler{
        .read = [&](u32 offset, AccessWidth width) -> Result<u32> {
            read_count++;
            last_offset = offset;
            last_width = width;
            if (width == AccessWidth::Byte)
                return 0x42;
            if (width == AccessWidth::Word)
                return 0x1234;
            if (width == AccessWidth::Dword)
                return 0x87654321;
            return Error{ErrorCode::UnsupportedAccessSize, "Largura não suportada", offset};
        },
        .write = [&](u32 offset, AccessWidth width, u32 value) -> Result<void> {
            write_count++;
            last_offset = offset;
            last_width = width;
            last_val = value;
            if (width == AccessWidth::Byte && offset == 0x0A) {
                return Error{ErrorCode::InvalidArgument, "Offset reservado", offset};
            }
            return Result<void>::Ok();
        }};

    // Mapeia MMIO em [0xF0000000, 0xF0001000)
    EXPECT_TRUE(as.MapMmio(0xF0000000, 0x1000, handler, MemoryPermission::ReadWrite).has_value());

    // Leitura 8 bits
    auto r8 = as.Read8(0xF0000004);
    EXPECT_TRUE(r8.has_value());
    EXPECT_EQ(r8.value(), 0x42);
    EXPECT_EQ(last_offset, 0x04U); // Offset relativo!
    EXPECT_EQ(static_cast<u8>(last_width), static_cast<u8>(AccessWidth::Byte));
    EXPECT_EQ(read_count, 1);

    // Leitura 32 bits
    auto r32 = as.Read32(0xF0000020);
    EXPECT_TRUE(r32.has_value());
    EXPECT_EQ(r32.value(), 0x87654321U);
    EXPECT_EQ(last_offset, 0x20U);
    EXPECT_EQ(read_count, 2);

    // Escrita 16 bits
    EXPECT_TRUE(as.Write16(0xF0000050, 0xABCD).has_value());
    EXPECT_EQ(last_offset, 0x50U);
    EXPECT_EQ(last_val, 0xABCDU);
    EXPECT_EQ(write_count, 1);

    // Erro propagado do MMIO sem fallback
    auto w_err = as.Write8(0xF000000A, 0xFF);
    EXPECT_FALSE(w_err.has_value());
    EXPECT_EQ(w_err.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(write_count, 2);
}

int main() {
    return xblob::testing::RunAllTests();
}
