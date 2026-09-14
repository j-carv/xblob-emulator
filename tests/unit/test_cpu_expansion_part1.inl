// Extended CPU tests - Part 1: Prefixes, Shifts, Mul/Div, Extensions

TEST_CASE(TestPrefixesAndUnsupportedMetadata) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // 1. Conflicting prefixes: 0xF2 (REPNE) and 0xF3 (REP)
    u8 conflict_bytes[] = {0xF2, 0xF3, 0x90};
    EXPECT_TRUE(
        ram.WriteBytes(0x1000, ByteSpan{conflict_bytes, sizeof(conflict_bytes)}).has_value());
    cpu.context().eip = 0x1000;
    auto s_conflict = cpu.Step(mem);
    EXPECT_EQ(s_conflict.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_unsupported().has_value());
    EXPECT_EQ(cpu.context().eip, 0x1000U); // Atomic, not advanced

    // 2. Unsupported 16-bit address size prefix: 0x67
    u8 addr_size_bytes[] = {0x67, 0x90};
    EXPECT_TRUE(
        ram.WriteBytes(0x1010, ByteSpan{addr_size_bytes, sizeof(addr_size_bytes)}).has_value());
    cpu.Reset();
    cpu.context().eip = 0x1010;
    auto s_addr = cpu.Step(mem);
    EXPECT_EQ(s_addr.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_unsupported().has_value());
    EXPECT_EQ(cpu.last_unsupported()->fault_eip, 0x1010U);

    // 3. Unsupported 16-bit operand size prefix: 0x66
    u8 op_size_bytes[] = {0x66, 0x90};
    EXPECT_TRUE(ram.WriteBytes(0x1020, ByteSpan{op_size_bytes, sizeof(op_size_bytes)}).has_value());
    cpu.Reset();
    cpu.context().eip = 0x1020;
    auto s_op = cpu.Step(mem);
    EXPECT_EQ(s_op.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_unsupported().has_value());

    // 4. Repeat prefix on non-string instruction: 0xF3 0x90
    u8 rep_nop_bytes[] = {0xF3, 0x90};
    EXPECT_TRUE(ram.WriteBytes(0x1030, ByteSpan{rep_nop_bytes, sizeof(rep_nop_bytes)}).has_value());
    cpu.Reset();
    cpu.context().eip = 0x1030;
    auto s_rep_nop = cpu.Step(mem);
    EXPECT_EQ(s_rep_nop.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_unsupported().has_value());

    // 5. LOCK on non-lockable instruction: 0xF0 0x90
    u8 lock_nop_bytes[] = {0xF0, 0x90};
    EXPECT_TRUE(
        ram.WriteBytes(0x1040, ByteSpan{lock_nop_bytes, sizeof(lock_nop_bytes)}).has_value());
    cpu.Reset();
    cpu.context().eip = 0x1040;
    auto s_lock_nop = cpu.Step(mem);
    EXPECT_EQ(s_lock_nop.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_exception().has_value());
    EXPECT_EQ(cpu.last_exception()->vector, ExceptionVector::InvalidOpcode);
}

TEST_CASE(TestShiftAndRotateInstructions) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // 1. SHL EAX, 1 (0xD1, 0xE0)
    u8 shl_1[] = {0xD1, 0xE0};
    EXPECT_TRUE(ram.WriteBytes(0x2000, ByteSpan{shl_1, sizeof(shl_1)}).has_value());
    cpu.context().eip = 0x2000;
    cpu.context().SetGpr(Reg32::EAX, 0x40000000U);
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x80000000U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagSF));
    EXPECT_FALSE(cpu.context().GetFlag(kFlagZF));
    EXPECT_FALSE(cpu.context().GetFlag(kFlagCF));
    EXPECT_TRUE(cpu.context().GetFlag(kFlagOF)); // bit31 changed from 0 to 1 -> OF=1

    // 2. SHL with count zero: EAX and flags untouched
    // 0xC1, 0xE0, 0x00 (SHL EAX, 0)
    u8 shl_0[] = {0xC1, 0xE0, 0x00};
    EXPECT_TRUE(ram.WriteBytes(0x2010, ByteSpan{shl_0, sizeof(shl_0)}).has_value());
    cpu.context().eip = 0x2010;
    cpu.context().SetGpr(Reg32::EAX, 0x12345678U);
    cpu.context().eflags = 0x246; // SF=0, ZF=1, PF=1, CF=0
    auto s2 = cpu.Step(mem);
    EXPECT_EQ(s2.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x12345678U);
    EXPECT_EQ(cpu.context().eflags, 0x246U); // Exactly preserved

    // 3. SAR EDX, CL (0xD3, 0xFA)
    u8 sar_cl[] = {0xD3, 0xFA};
    EXPECT_TRUE(ram.WriteBytes(0x2020, ByteSpan{sar_cl, sizeof(sar_cl)}).has_value());
    cpu.context().eip = 0x2020;
    cpu.context().SetGpr(Reg32::EDX, 0x80000000U);
    cpu.context().SetGpr(Reg32::ECX, 4);
    auto s3 = cpu.Step(mem);
    EXPECT_EQ(s3.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDX), 0xF8000000U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagSF));

    // 4. SHR EAX, 4 (0xC1, 0xE8, 0x04)
    u8 shr_4[] = {0xC1, 0xE8, 0x04};
    EXPECT_TRUE(ram.WriteBytes(0x2030, ByteSpan{shr_4, sizeof(shr_4)}).has_value());
    cpu.context().eip = 0x2030;
    cpu.context().SetGpr(Reg32::EAX, 0x00000010U);
    auto s4 = cpu.Step(mem);
    EXPECT_EQ(s4.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x00000001U);
    EXPECT_FALSE(cpu.context().GetFlag(kFlagCF));

    // 5. ROL EAX, 1 (0xD1, 0xC0)
    u8 rol_1[] = {0xD1, 0xC0};
    EXPECT_TRUE(ram.WriteBytes(0x2040, ByteSpan{rol_1, sizeof(rol_1)}).has_value());
    cpu.context().eip = 0x2040;
    cpu.context().SetGpr(Reg32::EAX, 0x80000001U);
    auto s5 = cpu.Step(mem);
    EXPECT_EQ(s5.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x00000003U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagCF));

    // 6. ROR EAX, 1 (0xD1, 0xC8)
    u8 ror_1[] = {0xD1, 0xC8};
    EXPECT_TRUE(ram.WriteBytes(0x2050, ByteSpan{ror_1, sizeof(ror_1)}).has_value());
    cpu.context().eip = 0x2050;
    cpu.context().SetGpr(Reg32::EAX, 0x00000001U);
    auto s6 = cpu.Step(mem);
    EXPECT_EQ(s6.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x80000000U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagCF));
}

TEST_CASE(TestMulAndImulInstructions) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // 1. MUL EBX (0xF7, 0x23 modrm reg=4, rm=3)
    u8 mul_ebx[] = {0xF7, 0xE3};
    EXPECT_TRUE(ram.WriteBytes(0x3000, ByteSpan{mul_ebx, sizeof(mul_ebx)}).has_value());
    cpu.context().eip = 0x3000;
    cpu.context().SetGpr(Reg32::EAX, 0x10000U);
    cpu.context().SetGpr(Reg32::EBX, 0x20000U);
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x00000000U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDX), 0x00000002U);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagCF));
    EXPECT_TRUE(cpu.context().GetFlag(kFlagOF));

    // 2. IMUL 2-operand: IMUL EAX, EBX (0x0F, 0xAF, 0xC3)
    u8 imul_2op[] = {0x0F, 0xAF, 0xC3};
    EXPECT_TRUE(ram.WriteBytes(0x3010, ByteSpan{imul_2op, sizeof(imul_2op)}).has_value());
    cpu.context().eip = 0x3010;
    cpu.context().SetGpr(Reg32::EAX, static_cast<u32>(-2));
    cpu.context().SetGpr(Reg32::EBX, 5);
    auto s2 = cpu.Step(mem);
    EXPECT_EQ(s2.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), static_cast<u32>(-10));
    EXPECT_FALSE(cpu.context().GetFlag(kFlagCF));
    EXPECT_FALSE(cpu.context().GetFlag(kFlagOF));

    // 3. IMUL 3-operand: IMUL EAX, EBX, imm8 (-4) (0x6B, 0xC3, 0xFC)
    u8 imul_3op[] = {0x6B, 0xC3, 0xFC};
    EXPECT_TRUE(ram.WriteBytes(0x3020, ByteSpan{imul_3op, sizeof(imul_3op)}).has_value());
    cpu.context().eip = 0x3020;
    cpu.context().SetGpr(Reg32::EBX, 10);
    auto s3 = cpu.Step(mem);
    EXPECT_EQ(s3.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), static_cast<u32>(-40));
}

TEST_CASE(TestDivAndIdivDivideErrors) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // 1. DIV EBX with divisor == 0 -> #DE exception without mutating context
    // 0xF7, 0xF3 (DIV EBX: reg=6)
    u8 div_ebx[] = {0xF7, 0xF3};
    EXPECT_TRUE(ram.WriteBytes(0x4000, ByteSpan{div_ebx, sizeof(div_ebx)}).has_value());
    cpu.context().eip = 0x4000;
    cpu.context().SetGpr(Reg32::EAX, 100);
    cpu.context().SetGpr(Reg32::EDX, 0);
    cpu.context().SetGpr(Reg32::EBX, 0); // Divide by zero!
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_exception().has_value());
    EXPECT_EQ(cpu.last_exception()->vector, ExceptionVector::DivideError);
    // Atomic preservation:
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 100U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDX), 0U);
    EXPECT_EQ(cpu.context().eip, 0x4000U);

    // 2. DIV EBX with quotient overflow (dividend = 0x1_00000000, divisor = 1) -> #DE
    cpu.Reset();
    cpu.context().eip = 0x4000;
    cpu.context().SetGpr(Reg32::EAX, 0);
    cpu.context().SetGpr(Reg32::EDX, 1);
    cpu.context().SetGpr(Reg32::EBX, 1);
    auto s2 = cpu.Step(mem);
    EXPECT_EQ(s2.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_exception().has_value());
    EXPECT_EQ(cpu.last_exception()->vector, ExceptionVector::DivideError);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDX), 1U);

    // 3. IDIV EBX host UB check (INT64_MIN / -1)
    // 0xF7, 0xFB (IDIV EBX: reg=7)
    u8 idiv_ebx[] = {0xF7, 0xFB};
    EXPECT_TRUE(ram.WriteBytes(0x4010, ByteSpan{idiv_ebx, sizeof(idiv_ebx)}).has_value());
    cpu.Reset();
    cpu.context().eip = 0x4010;
    cpu.context().SetGpr(Reg32::EAX, 0);
    cpu.context().SetGpr(Reg32::EDX, 0x80000000U);
    cpu.context().SetGpr(Reg32::EBX, 0xFFFFFFFFU); // -1
    auto s3 = cpu.Step(mem);
    EXPECT_EQ(s3.result, StepResult::Faulted);
    EXPECT_TRUE(cpu.last_exception().has_value());
    EXPECT_EQ(cpu.last_exception()->vector, ExceptionVector::DivideError);

    // 4. Normal IDIV
    cpu.Reset();
    cpu.context().eip = 0x4010;
    cpu.context().SetGpr(Reg32::EAX, static_cast<u32>(-20));
    cpu.context().SetGpr(Reg32::EDX, 0xFFFFFFFFU); // sign-extended
    cpu.context().SetGpr(Reg32::EBX, 3);
    auto s4 = cpu.Step(mem);
    EXPECT_EQ(s4.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), static_cast<u32>(-6));
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDX), static_cast<u32>(-2));
}

TEST_CASE(TestBitAndExtensionInstructions) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // 1. MOVZX EAX, byte [0x5000] (0x0F, 0xB6, 0x05, 0x00, 0x50, 0x00, 0x00)
    u8 movzx_byte[] = {0x0F, 0xB6, 0x05, 0x00, 0x50, 0x00, 0x00};
    EXPECT_TRUE(ram.WriteBytes(0x1000, ByteSpan{movzx_byte, sizeof(movzx_byte)}).has_value());
    EXPECT_TRUE(ram.Write8(0x5000, 0xFE).has_value());
    cpu.context().eip = 0x1000;
    cpu.context().SetGpr(Reg32::EAX, 0xFFFFFFFFU);
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x000000FEU);

    // 2. MOVSX EAX, byte [0x5000] (0x0F, 0xBE, 0x05, 0x00, 0x50, 0x00, 0x00)
    u8 movsx_byte[] = {0x0F, 0xBE, 0x05, 0x00, 0x50, 0x00, 0x00};
    EXPECT_TRUE(ram.WriteBytes(0x1010, ByteSpan{movsx_byte, sizeof(movsx_byte)}).has_value());
    cpu.context().eip = 0x1010;
    cpu.context().SetGpr(Reg32::EAX, 0);
    auto s2 = cpu.Step(mem);
    EXPECT_EQ(s2.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0xFFFFFFFEU); // Sign-extended

    // 3. CDQ (0x99)
    u8 cdq[] = {0x99};
    EXPECT_TRUE(ram.WriteBytes(0x1020, ByteSpan{cdq, sizeof(cdq)}).has_value());
    cpu.context().eip = 0x1020;
    cpu.context().SetGpr(Reg32::EAX, 0x80000000U);
    auto s3 = cpu.Step(mem);
    EXPECT_EQ(s3.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDX), 0xFFFFFFFFU);

    // 4. BT EAX, 3 (0x0F, 0xBA, 0xE0, 0x03)
    u8 bt_imm[] = {0x0F, 0xBA, 0xE0, 0x03};
    EXPECT_TRUE(ram.WriteBytes(0x1030, ByteSpan{bt_imm, sizeof(bt_imm)}).has_value());
    cpu.context().eip = 0x1030;
    cpu.context().SetGpr(Reg32::EAX, 0x08U); // bit 3 is set
    auto s4 = cpu.Step(mem);
    EXPECT_EQ(s4.result, StepResult::Ok);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagCF));

    // 5. SETE AL (0x0F, 0x94, 0xC0)
    u8 sete[] = {0x0F, 0x94, 0xC0};
    EXPECT_TRUE(ram.WriteBytes(0x1040, ByteSpan{sete, sizeof(sete)}).has_value());
    cpu.context().eip = 0x1040;
    cpu.context().SetGpr(Reg32::EAX, 0xFFFFFFFFU);
    cpu.context().SetFlag(kFlagZF, true);
    auto s5 = cpu.Step(mem);
    EXPECT_EQ(s5.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr8(0), 1);
}
