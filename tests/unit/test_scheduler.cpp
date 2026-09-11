#include "tests/test_framework.hpp"
#include "xblob/common/error.hpp"
#include "xblob/core/scheduler.hpp"

#include <vector>

using namespace xblob;
using namespace xblob::core;

// Task 3.1: Relógio monotônico uint64_t, avanço verificado, overflow, agendamento no passado
TEST_CASE(TestSchedulerAdvanceAndOverflow) {
    DeterministicScheduler sched(10);
    EXPECT_EQ(sched.current_cycle(), 10ULL);

    // Avanço explícito de 25 ciclos a partir de 10 -> 35
    auto adv = sched.AdvanceCycles(25);
    EXPECT_TRUE(adv.has_value());
    EXPECT_EQ(sched.current_cycle(), 35ULL);

    // Alvo no passado
    auto past_res = sched.ScheduleEvent(30, [](Cycle) {});
    EXPECT_FALSE(past_res.has_value());
    EXPECT_EQ(past_res.error().code, ErrorCode::PastCycle);

    // Overflow de tempo
    auto of_adv = sched.AdvanceCycles(std::numeric_limits<Cycle>::max());
    EXPECT_FALSE(of_adv.has_value());
    EXPECT_EQ(of_adv.error().code, ErrorCode::IntegerOverflow);
    EXPECT_EQ(sched.current_cycle(), 35ULL); // Intacto
}

// Task 3.2: Despacho por (ciclo, sequência) com ordem estável em empates, fora de ordem, futuros
TEST_CASE(TestSchedulerDispatchOrder) {
    DeterministicScheduler sched(0);
    std::vector<int> executed_order;

    // Inserir eventos fora de ordem de ciclo: ciclo 20, depois ciclo 10, depois ciclo 5
    EXPECT_TRUE(sched.ScheduleEvent(20, [&](Cycle) { executed_order.push_back(20); }).has_value());
    EXPECT_TRUE(sched.ScheduleEvent(10, [&](Cycle) { executed_order.push_back(10); }).has_value());
    EXPECT_TRUE(sched.ScheduleEvent(5, [&](Cycle) { executed_order.push_back(5); }).has_value());

    // Eventos simultâneos no ciclo 15 inseridos em ordem A, B, C
    EXPECT_TRUE(sched.ScheduleEvent(15, [&](Cycle) { executed_order.push_back(151); }).has_value());
    EXPECT_TRUE(sched.ScheduleEvent(15, [&](Cycle) { executed_order.push_back(152); }).has_value());
    EXPECT_TRUE(sched.ScheduleEvent(15, [&](Cycle) { executed_order.push_back(153); }).has_value());

    // Executa até ciclo 12: deve executar 5 e 10, mas NÃO 15 ou 20
    auto step1 = sched.RunUntil(12);
    EXPECT_TRUE(step1.has_value());
    EXPECT_EQ(step1.value(), SchedulerStatus::TargetReached);
    EXPECT_EQ(sched.current_cycle(), 12ULL);
    EXPECT_EQ(executed_order.size(), 2ULL);
    EXPECT_EQ(executed_order[0], 5);
    EXPECT_EQ(executed_order[1], 10);

    // Executa até ciclo 25: deve executar os três eventos do ciclo 15 na ordem estável de inserção
    // e depois 20
    auto step2 = sched.RunUntil(25);
    EXPECT_TRUE(step2.has_value());
    EXPECT_EQ(step2.value(), SchedulerStatus::TargetReached);
    EXPECT_EQ(sched.current_cycle(), 25ULL);
    EXPECT_EQ(executed_order.size(), 6ULL);
    EXPECT_EQ(executed_order[2], 151);
    EXPECT_EQ(executed_order[3], 152);
    EXPECT_EQ(executed_order[4], 153);
    EXPECT_EQ(executed_order[5], 20);
}

// Task 3.3: Cancelamento por id opaco sem perturbar eventos restantes
TEST_CASE(TestSchedulerCancellation) {
    DeterministicScheduler sched(0);
    std::vector<int> executed;

    auto id1 = sched.ScheduleEvent(10, [&](Cycle) { executed.push_back(1); });
    auto id2 = sched.ScheduleEvent(10, [&](Cycle) { executed.push_back(2); });
    auto id3 = sched.ScheduleEvent(10, [&](Cycle) { executed.push_back(3); });
    EXPECT_TRUE(id1.has_value());
    EXPECT_TRUE(id2.has_value());
    EXPECT_TRUE(id3.has_value());
    EXPECT_EQ(sched.pending_event_count(), 3ULL);

    // Cancelar id2
    auto cancel_res = sched.CancelEvent(id2.value());
    EXPECT_TRUE(cancel_res.has_value());
    EXPECT_EQ(sched.pending_event_count(), 2ULL);

    // Cancelar id repetido deve falhar com EventNotFound
    auto cancel_rep = sched.CancelEvent(id2.value());
    EXPECT_FALSE(cancel_rep.has_value());
    EXPECT_EQ(cancel_rep.error().code, ErrorCode::EventNotFound);

    // Cancelar id inexistente deve falhar com EventNotFound
    auto cancel_unknown = sched.CancelEvent(999999ULL);
    EXPECT_FALSE(cancel_unknown.has_value());
    EXPECT_EQ(cancel_unknown.error().code, ErrorCode::EventNotFound);

    // Executar ciclo 10: apenas id1 e id3 devem rodar, na ordem relativa preservada
    EXPECT_TRUE(sched.RunUntil(10).has_value());
    EXPECT_EQ(executed.size(), 2ULL);
    EXPECT_EQ(executed[0], 1);
    EXPECT_EQ(executed[1], 3);
}

// Task 3.4: Execução com limites de ciclo e quantidade de eventos, reagendamento no ciclo atual
TEST_CASE(TestSchedulerLimitsAndImmediateRescheduling) {
    DeterministicScheduler sched(0);
    int fire_count = 0;

    // Callback que se reagenda imediatamente no ciclo atual
    std::function<void(Cycle)> self_rescheduler;
    self_rescheduler = [&](Cycle c) {
        fire_count++;
        // Reagenda no mesmo ciclo
        (void)sched.ScheduleEvent(c, self_rescheduler);
    };

    EXPECT_TRUE(sched.ScheduleEvent(0, self_rescheduler).has_value());

    // Executa com limite de 10 eventos
    auto res = sched.RunUntil(100, 10);
    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), SchedulerStatus::EventLimitReached);
    EXPECT_EQ(fire_count, 10);
    EXPECT_EQ(sched.current_cycle(), 0ULL); // Não travou em loop infinito
}

int main() {
    return xblob::testing::RunAllTests();
}
