// Extended CPU tests - Part 2: Strings, REP micro-steps, Atomic, Memory faults

TEST_CASE(TestStringInstructionsAndDf) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // 1. MOVSB with DF=0
    // 0xA4
    u8 movsb[] = {0xA4};
    EXPECT_TRUE(ram.WriteBytes(0x1000, ByteSpan{movsb, sizeof(movsb)}).has_value());
    EXPECT_TRUE(ram.Write8(0x2000, 0x42).has_value());
    cpu.context().eip = 0x1000;
    cpu.context().SetGpr(Reg32::ESI, 0x2000);
    cpu.context().SetGpr(Reg32::EDI, 0x3000);
    cpu.context().SetFlag(kFlagDF, false);
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Ok);
    auto r1 = mem.Read8(0x3000);
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(*r1, 0x42U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESI), 0x2001U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDI), 0x3001U);

    // 2. MOVSD with DF=1 (STD)
    // 0xFD (STD), 0xA5 (MOVSD)
    u8 std_movsd[] = {0xFD, 0xA5};
    EXPECT_TRUE(ram.WriteBytes(0x1010, ByteSpan{std_movsd, sizeof(std_movsd)}).has_value());
    EXPECT_TRUE(mem.Write32(0x2010, 0xCAFEBABE).has_value());
    cpu.context().eip = 0x1010;
    cpu.context().SetGpr(Reg32::ESI, 0x2010);
    cpu.context().SetGpr(Reg32::EDI, 0x3010);
    // Execute STD:
    auto s_std = cpu.Step(mem);
    EXPECT_EQ(s_std.result, StepResult::Ok);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagDF));
    // Execute MOVSD:
    auto s_movsd = cpu.Step(mem);
    EXPECT_EQ(s_movsd.result, StepResult::Ok);
    auto r_movsd = mem.Read32(0x3010);
    EXPECT_TRUE(r_movsd.has_value());
    EXPECT_EQ(*r_movsd, 0xCAFEBABEU);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESI), 0x2010U - 4U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDI), 0x3010U - 4U);

    // 3. STOSD
    // 0xAB
    u8 stosd[] = {0xAB};
    EXPECT_TRUE(ram.WriteBytes(0x1020, ByteSpan{stosd, sizeof(stosd)}).has_value());
    cpu.context().eip = 0x1020;
    cpu.context().SetFlag(kFlagDF, false);
    cpu.context().SetGpr(Reg32::EAX, 0x11223344);
    cpu.context().SetGpr(Reg32::EDI, 0x5000);
    auto s_stos = cpu.Step(mem);
    EXPECT_EQ(s_stos.result, StepResult::Ok);
    auto r_stos = mem.Read32(0x5000);
    EXPECT_TRUE(r_stos.has_value());
    EXPECT_EQ(*r_stos, 0x11223344U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDI), 0x5004U);

    // 4. CMPSB
    // 0xA6
    u8 cmpsb[] = {0xA6};
    EXPECT_TRUE(ram.WriteBytes(0x1030, ByteSpan{cmpsb, sizeof(cmpsb)}).has_value());
    EXPECT_TRUE(ram.Write8(0x6000, 0x10).has_value());
    EXPECT_TRUE(ram.Write8(0x7000, 0x10).has_value());
    cpu.context().eip = 0x1030;
    cpu.context().SetGpr(Reg32::ESI, 0x6000);
    cpu.context().SetGpr(Reg32::EDI, 0x7000);
    auto s_cmps = cpu.Step(mem);
    EXPECT_EQ(s_cmps.result, StepResult::Ok);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagZF));
}

TEST_CASE(TestRepMicroSteppingAndResumption) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    EXPECT_TRUE(mem.MapRam(0x0, 0x10000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // REP MOVSB: 0xF3, 0xA4
    // Copy 5 bytes from 0x2000 to 0x3000
    u8 rep_movsb[] = {0xF3, 0xA4};
    EXPECT_TRUE(ram.WriteBytes(0x1000, ByteSpan{rep_movsb, sizeof(rep_movsb)}).has_value());
    u8 src_data[] = {1, 2, 3, 4, 5};
    EXPECT_TRUE(ram.WriteBytes(0x2000, ByteSpan{src_data, sizeof(src_data)}).has_value());

    cpu.context().eip = 0x1000;
    cpu.context().SetGpr(Reg32::ESI, 0x2000);
    cpu.context().SetGpr(Reg32::EDI, 0x3000);
    cpu.context().SetGpr(Reg32::ECX, 5);
    cpu.context().SetFlag(kFlagDF, false);

    // Run with budget = 2 instructions (should execute exactly 2 micro-steps)
    auto out1 = cpu.RunWithBudget(mem, 2);
    EXPECT_EQ(out1.status, RunStatus::BudgetExhausted);
    EXPECT_EQ(out1.instructions_executed, 2U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ECX), 3U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESI), 0x2002U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDI), 0x3002U);
    EXPECT_EQ(cpu.context().eip, 0x1000U); // EIP remains at REP instruction!

    // Verify first 2 bytes were copied:
    auto b0 = mem.Read8(0x3000);
    auto b1 = mem.Read8(0x3001);
    EXPECT_TRUE(b0.has_value() && *b0 == 1);
    EXPECT_TRUE(b1.has_value() && *b1 == 2);

    // Resume execution with budget = 10: place HLT at 0x1002
    EXPECT_TRUE(ram.Write8(0x1002, 0xF4).has_value());
    auto out2 = cpu.RunWithBudget(mem, 10);
    EXPECT_EQ(out2.status, RunStatus::Halted);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ECX), 0U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESI), 0x2005U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDI), 0x3005U);
    EXPECT_EQ(cpu.context().eip, 0x1003U); // HLT executed at 0x1002, advancing EIP to 0x1003

    // All 5 bytes copied:
    for (u32 i = 0; i < 5; ++i) {
        auto val = mem.Read8(0x3000 + i);
        EXPECT_TRUE(val.has_value());
        EXPECT_EQ(*val, src_data[i]);
    }
}

TEST_CASE(TestAtomicAndMemoryFaults) {
    auto ram_res = Ram::Create(kRamSizeRetail);
    EXPECT_TRUE(ram_res.has_value());
    Ram& ram = ram_res.value();
    AddressSpace mem;
    // Map 0x0000..0x3000; 0x4000..0xFFFF is unmapped
    EXPECT_TRUE(mem.MapRam(0x0, 0x3000, ram, 0, MemoryPermission::All).has_value());

    Cpu cpu;

    // 1. XCHG EAX, EBX (0x93)
    u8 xchg_ab[] = {0x93};
    EXPECT_TRUE(ram.WriteBytes(0x1000, ByteSpan{xchg_ab, sizeof(xchg_ab)}).has_value());
    cpu.context().eip = 0x1000;
    cpu.context().SetGpr(Reg32::EAX, 0xAAAAAAAAU);
    cpu.context().SetGpr(Reg32::EBX, 0xBBBBBBBBU);
    auto s1 = cpu.Step(mem);
    EXPECT_EQ(s1.result, StepResult::Ok);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0xBBBBBBBBU);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EBX), 0xAAAAAAAAU);

    // 2. CMPXCHG [0x2000], EBX (match)
    // 0x0F, 0xB1, 0x1D, 0x00, 0x20, 0x00, 0x00
    u8 cmpxchg_match[] = {0x0F, 0xB1, 0x1D, 0x00, 0x20, 0x00, 0x00};
    EXPECT_TRUE(ram.WriteBytes(0x1010, ByteSpan{cmpxchg_match, sizeof(cmpxchg_match)}).has_value());
    EXPECT_TRUE(mem.Write32(0x2000, 0x55555555U).has_value());
    cpu.context().eip = 0x1010;
    cpu.context().SetGpr(Reg32::EAX, 0x55555555U); // matches memory!
    cpu.context().SetGpr(Reg32::EBX, 0x99999999U);
    auto s2 = cpu.Step(mem);
    EXPECT_EQ(s2.result, StepResult::Ok);
    EXPECT_TRUE(cpu.context().GetFlag(kFlagZF));
    auto r_mem = mem.Read32(0x2000);
    EXPECT_TRUE(r_mem.has_value());
    EXPECT_EQ(*r_mem, 0x99999999U); // Updated!

    // 3. CMPXCHG [0x2000], EBX (mismatch)
    cpu.context().eip = 0x1010;
    cpu.context().SetGpr(Reg32::EAX, 0x11111111U); // mismatch!
    cpu.context().SetGpr(Reg32::EBX, 0x88888888U);
    auto s3 = cpu.Step(mem);
    EXPECT_EQ(s3.result, StepResult::Ok);
    EXPECT_FALSE(cpu.context().GetFlag(kFlagZF));
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EAX), 0x99999999U); // Loaded old memory value
    auto r_mem2 = mem.Read32(0x2000);
    EXPECT_TRUE(r_mem2.has_value());
    EXPECT_EQ(*r_mem2, 0x99999999U); // Unmodified!

    // 4. Fault during string instruction (unmapped src address)
    // 0xA4 (MOVSB)
    u8 movsb[] = {0xA4};
    EXPECT_TRUE(ram.WriteBytes(0x1030, ByteSpan{movsb, sizeof(movsb)}).has_value());
    cpu.context().eip = 0x1030;
    cpu.context().SetGpr(Reg32::ESI, 0x8000); // UNMAPPED!
    cpu.context().SetGpr(Reg32::EDI, 0x2500);
    auto s_fault = cpu.Step(mem);
    EXPECT_EQ(s_fault.result, StepResult::Faulted);
    // EIP, ESI, EDI preserved atomically:
    EXPECT_EQ(cpu.context().eip, 0x1030U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::ESI), 0x8000U);
    EXPECT_EQ(cpu.context().GetGpr(Reg32::EDI), 0x2500U);
}
