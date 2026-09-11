#include "tests/test_framework.hpp"
#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/safe_math.hpp"
#include "xblob/common/types.hpp"

using namespace xblob;

TEST_CASE(TestSafeMathCheckedAdd) {
    u64 out = 0;
    EXPECT_TRUE(CheckedAddU64(10, 20, out));
    EXPECT_EQ(out, 30ULL);

    EXPECT_TRUE(CheckedAddU64(0, 0, out));
    EXPECT_EQ(out, 0ULL);

    // Overflow check
    EXPECT_FALSE(CheckedAddU64(std::numeric_limits<u64>::max(), 1ULL, out));
    EXPECT_FALSE(CheckedAddU64(std::numeric_limits<u64>::max() / 2 + 10,
                               std::numeric_limits<u64>::max() / 2 + 10, out));
}

TEST_CASE(TestSafeMathCheckedMul) {
    u64 out = 0;
    EXPECT_TRUE(CheckedMulU64(10, 20, out));
    EXPECT_EQ(out, 200ULL);

    EXPECT_TRUE(CheckedMulU64(0, std::numeric_limits<u64>::max(), out));
    EXPECT_EQ(out, 0ULL);

    EXPECT_FALSE(CheckedMulU64(std::numeric_limits<u64>::max(), 2ULL, out));
    EXPECT_FALSE(CheckedMulU64(0x100000000ULL, 0x100000000ULL, out));
}

TEST_CASE(TestSafeMathRangeInBounds) {
    EXPECT_TRUE(RangeInBoundsU64(0, 100, 100));
    EXPECT_TRUE(RangeInBoundsU64(10, 90, 100));
    EXPECT_TRUE(RangeInBoundsU64(0, 0, 100));

    // Exceeds total size
    EXPECT_FALSE(RangeInBoundsU64(0, 101, 100));
    EXPECT_FALSE(RangeInBoundsU64(10, 91, 100));

    // Overflow in offset + size
    EXPECT_FALSE(RangeInBoundsU64(std::numeric_limits<u64>::max() - 10, 20, 1000));
}

TEST_CASE(TestResultSuccessAndError) {
    Result<int> res_ok(42);
    EXPECT_TRUE(res_ok.has_value());
    EXPECT_TRUE(static_cast<bool>(res_ok));
    EXPECT_EQ(*res_ok, 42);
    EXPECT_EQ(res_ok.value_or(10), 42);

    Result<int> res_err(Error{ErrorCode::FileNotFound, "Arquivo não encontrado", 0});
    EXPECT_FALSE(res_err.has_value());
    EXPECT_FALSE(static_cast<bool>(res_err));
    EXPECT_EQ(res_err.error().code, ErrorCode::FileNotFound);
    EXPECT_EQ(res_err.value_or(99), 99);
}

TEST_CASE(TestResultVoid) {
    Result<void> v_ok = Result<void>::Ok();
    EXPECT_TRUE(v_ok.has_value());
    EXPECT_TRUE(static_cast<bool>(v_ok));

    Result<void> v_err(Error{ErrorCode::OutOfBounds, "Limite excedido", 128});
    EXPECT_FALSE(v_err.has_value());
    EXPECT_EQ(v_err.error().code, ErrorCode::OutOfBounds);
    EXPECT_EQ(v_err.error().offset, 128ULL);
}

TEST_CASE(TestSafeMathCheckedSub) {
    u64 out = 0;
    EXPECT_TRUE(CheckedSubU64(30, 10, out));
    EXPECT_EQ(out, 20ULL);

    EXPECT_TRUE(CheckedSubU64(10, 10, out));
    EXPECT_EQ(out, 0ULL);

    // Underflow check
    EXPECT_FALSE(CheckedSubU64(10, 20, out));
    EXPECT_FALSE(CheckedSubU64(0, 1, out));
}

TEST_CASE(TestErrorCodeToString) {
    EXPECT_EQ(ErrorCodeToString(ErrorCode::Ok), "Ok");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::FileNotFound), "Arquivo não encontrado");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::InvalidMagic), "Assinatura mágica inválida");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::UnmappedAddress), "Endereço não mapeado");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::AccessViolation), "Violação de permissão de acesso");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::RegionOverlap), "Sobreposição de regiões de memória");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::InvalidCapacity), "Capacidade de memória inválida");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::PastCycle), "Ciclo alvo no passado");
    EXPECT_EQ(ErrorCodeToString(ErrorCode::InvalidOpcode), "Opcode inválido");
}

int main() {
    return xblob::testing::RunAllTests();
}
