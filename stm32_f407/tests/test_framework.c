#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "emm_protocol.h"
#include "motor_state_machine.h"
#include "buzzer.h"
#include "motor_selftest.h"
#include "motor_startup.h"
#include "remote_control.h"
static bool beep;
static DMA_HandleTypeDef dma[3];
static UART_HandleTypeDef hal[3];
static motor_t motors[3];
void Buzzer_BSP_Set(bool on)
{
    beep = on;
}
void UART_BSP_NotifyFromISR(void)
{
}
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    (void)p;
    h->hdmarx->counter = n;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *h, uint8_t *p, uint16_t n)
{
    (void)h;
    (void)p;
    (void)n;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Abort(UART_HandleTypeDef *h)
{
    (void)h;
    return HAL_OK;
}
static void init(void)
{
    for (int i = 0; i < 3; i++)
    {
        hal[i].hdmarx = &dma[i];
        assert(UART_BSP_Start(&g_motor_ports[i], &hal[i]));
        Motor_Init(&motors[i], &g_motor_ports[i], 1, 0);
    }
}
static void feed(unsigned i, const uint8_t *b, size_t n)
{
    uart_port_t *p = &g_motor_ports[i];
    unsigned pos = (APP_UART_DMA_SIZE - dma[i].counter) % APP_UART_DMA_SIZE;
    for (size_t j = 0; j < n; j++)
    {
        p->dma[pos] = b[j];
        pos = (pos + 1) % APP_UART_DMA_SIZE;
        dma[i].counter = APP_UART_DMA_SIZE - pos;
        HAL_UARTEx_RxEventCallback(&hal[i], (uint16_t)pos);
    }
}
static void ack(unsigned i, uint8_t code, uint8_t status, uint32_t now)
{
    uint8_t b[] = {1, code, status, 0x6B};
    HAL_UART_TxCpltCallback(&hal[i]);
    feed(i, b, sizeof(b));
    Motor_Process(&motors[i], now);
}
static void ready(unsigned i, uint32_t now)
{
    motor_t *m = &motors[i];
    m->flags = 3;
    m->flags_valid = m->position_valid = m->origin_valid = m->enable_requested = true;
    m->flags_ms = m->position_ms = now;
    m->mode = MOTOR_HOLD;
}
static void protocol(void)
{
    uint8_t b[16];
    const uint8_t position[] = {1, 0xFD, 0, 0, 60, 10, 0, 0, 12, 128, 1, 0, 0x6B};
    assert(Emm_Position(b, 1, false, 60, 10, 3200, true) == 13);
    assert(!memcmp(b, position, 13));
    const uint8_t vel[] = {1, 0xF6, 1, 1, 224, 10, 0, 0x6B};
    assert(Emm_Velocity(b, 1, true, 480, 10) == 8);
    assert(!memcmp(b, vel, 8));
    assert(Emm_Velocity(b, 1, false, 5001, 10) == 0);
    const uint8_t enable[] = {1, 0xF3, 0xAB, 1, 0, 0x6B};
    assert(Emm_Enable(b, 1, true) == 6 && !memcmp(b, enable, 6));
    assert(Emm_Stop(b, 1) == 5 && b[2] == 0x98 && b[4] == 0x6B);
    assert(Emm_Zero(b, 1) == 4 && b[2] == 0x6D);
    emm_parser_t p = {0};
    emm_reply_t out;
    uint8_t stream[] = {0xFF, 2, 0x36, 0, 0, 0,    0, 0,    0x6B, 1,   0x36,
                        1,    0, 1,    0, 0, 0x6B, 1, 0xF3, 2,    0x6B};
    unsigned count = 0;
    for (unsigned i = 0; i < sizeof(stream); i++)
        if (Emm_ParseByte(&p, 1, stream[i], &out))
        {
            if (!count)
                assert(out.code == EMM_POSITION && out.position == -65536);
            else
                assert(out.code == EMM_ENABLE && out.status == 2);
            count++;
        }
    assert(count == 2);
    uint8_t bad[] = {1, 0xF3, 2, 0xFF, 1, 0xF3, 2, 0x6B};
    count = 0;
    for (unsigned i = 0; i < sizeof(bad); i++)
        count += Emm_ParseByte(&p, 1, bad[i], &out);
    assert(count == 1);
    puts("PASS protocol: golden packets, split/concatenated frames, noise, bad tail, signed "
         "feedback");
}
static void conversions(void)
{
    bool rev;
    uint32_t pulse;
    uint16_t rpm;
    assert(Axis_Convert(&g_axis_config[0], 1, 1, &rev, &pulse, &rpm));
    assert(pulse == 3200 && rpm == 60 && !rev);
    assert(Axis_Convert(&g_axis_config[2], -90, 90, &rev, &pulse, &rpm));
    assert(pulse == 800 && rpm == 15 && rev);
    assert(fabsf(Axis_Position(&g_axis_config[2], -65536) + 360) < 0.001f);
    assert(!Axis_Convert(&g_axis_config[0], 51, 1, &rev, &pulse, &rpm));
    assert(!Axis_Convert(&g_axis_config[0], NAN, 1, &rev, &pulse, &rpm));
    assert(!Axis_Convert(&g_axis_config[0], 1, 0, &rev, &pulse, &rpm));
    puts("PASS units: mm/pulses/RPM, yaw degrees, limits, NaN, zero speed");
}
static void transport(void)
{
    init();
    uint8_t b[3] = {3, 4, 5}, out[8];
    feed(0, b, 3);
    HAL_UARTEx_RxEventCallback(&hal[0], 64); /* stale HT event must not duplicate bytes */
    assert(UART_BSP_Read(&g_motor_ports[0], out, 8) == 3 && !memcmp(out, b, 3));
    for (unsigned i = 0; i < 200; i++)
    {
        feed(0, b, 3);
        assert(UART_BSP_Read(&g_motor_ports[0], out, 8) == 3);
    }
    assert(UART_BSP_Send(&g_motor_ports[0], b, 3));
    assert(!UART_BSP_Send(&g_motor_ports[0], b, 3));
    HAL_UART_TxCpltCallback(&hal[0]);
    assert(UART_BSP_Send(&g_motor_ports[0], b, 3));
    for (unsigned i = 0; i < 180; i++)
        feed(0, b, 3);
    assert(g_motor_ports[0].broken && g_motor_ports[0].dropped);
    assert(UART_BSP_Recover(&g_motor_ports[0]));
    assert(!g_motor_ports[0].tx_busy);
    puts("PASS UART: DMA wrap, duplicate events, busy TX, overflow, recovery");
}
static void transactions(void)
{
    init();
    ready(0, 1000);
    motor_t *m = &motors[0];
    assert(Motor_Move(m, false, 60, 10, 3200, 1000));
    assert(!Motor_Move(m, false, 60, 10, 6400, 1001));
    ack(0, EMM_MOVE, 0x9F, 1002);
    assert(m->pending == EMM_MOVE); /* not acceptance */
    ack(0, EMM_MOVE, 2, 1003);
    assert(!m->pending);
    assert(Motor_Move(m, false, 60, 10, 6400, 1010));
    Motor_Process(m, 1090);
    assert(m->fault == MOTOR_COMM_FAULT && !m->origin_valid && m->timeouts == 1);
    Motor_Process(m, 1091);
    Motor_Process(m, 1092);
    assert(m->pending == EMM_STOP && m->port->tx[1] == EMM_STOP); /* no movement retry */
    ack(0, EMM_STOP, 2, 1093);
    ready(0, 1093);
    assert(Motor_ClearFault(m, 1094));
    assert(Motor_Enable(m, true, 1100));
    ack(0, EMM_ENABLE, 0xEE, 1101);
    assert(m->fault == MOTOR_REJECTED);
    init();
    ready(0, 1000);
    m = &motors[0];
    Motor_RequestStop(m);
    Motor_Process(m, 1001);
    ack(0, EMM_STOP, 0xEE, 1002);
    assert(m->stop_failed && !Motor_ClearFault(m, 1003));
    puts("PASS transactions: ACK vs arrival, timeout, no motion replay, rejection, failed stop");
}
static void paths(void)
{
    init();
    ready(0, 1000);
    motion_state_t s = {0};
    motion_path_t p = {.mode = PATH_POINT_TO_POINT,
                       .axis_mask = 1,
                       .point_count = 1,
                       .cycles = 1,
                       .segment_timeout_ms = 1000};
    p.points[0].target[0] = 1;
    p.points[0].speed[0] = 1;
    assert(Motion_Start(&s, &p, motors, 1000));
    Motion_Update(&s, motors, 1001);
    assert(s.sent_mask == 1);
    ack(0, EMM_MOVE, 2, 1002);
    motors[0].position = 65536; /* stale feedback may not finish path */
    Motion_Update(&s, motors, 1003);
    assert(s.status == PATH_RUNNING);
    motors[0].position_ms = motors[0].flags_ms = 1004;
    Motion_Update(&s, motors, 1004);
    assert(s.status == PATH_DONE);
    p.mode = PATH_RECIPROCATING;
    p.point_count = 2;
    p.cycles = 2;
    p.points[0].target[0] = 0;
    p.points[1].target[0] = 1;
    p.points[1].speed[0] = 1;
    assert(Motion_Start(&s, &p, motors, 1010));
    for (unsigned n = 0; n < 4; n++)
    {
        uint32_t now = 1011 + n * 10;
        Motion_Update(&s, motors, now);
        ack(0, EMM_MOVE, 2, now + 1);
        motors[0].position = (n % 2) ? 65536 : 0;
        motors[0].position_ms = motors[0].flags_ms = now + 2;
        Motion_Update(&s, motors, now + 2);
    }
    assert(s.status == PATH_DONE && s.cycle == 2);
    assert(Motion_Start(&s, &p, motors, 1060));
    motors[0].flags_ms = 0;
    Motion_Update(&s, motors, 1061);
    assert(s.status == PATH_FAILED && motors[0].stop_requested && motors[1].stop_requested);
    p.points[1].target[0] = 100;
    assert(!Motion_Validate(&p));
    puts("PASS paths: point, reciprocating cycles, fresh feedback gate, fault propagation, "
         "whole-path validation");
}
static void buzzer(void)
{
    buzzer_t b;
    Buzzer_Init(&b);
    buzzer_pattern_t p = {10, 5, 2};
    assert(Buzzer_Play(&b, &p, UINT32_MAX - 4));
    assert(beep);
    Buzzer_Update(&b, 4);
    assert(beep);
    Buzzer_Update(&b, 5);
    assert(!beep);
    Buzzer_Update(&b, 10);
    assert(beep);
    Buzzer_Update(&b, 20);
    assert(!beep && !b.active);
    assert(Buzzer_Play(&b, &p, 30));
    Buzzer_Mute(&b, true);
    assert(!beep && !b.active);
    assert(!Buzzer_Play(&b, &p, 40));
    Buzzer_Mute(&b, false);
    assert(!b.active);
    puts("PASS buzzer: rhythm, finite repeats, time wrap, mute cancellation");
}

static void selftests(void)
{
    init();
    motor_selftest_t st;
    motion_state_t path = {0};
    motor_command_t cmd;
    SelfTest_Init(&st, 1000);
#if APP_MOTOR_SELFTEST
    assert(!SelfTest_Update(&st, motors, &path, 1000, &cmd));
    ready(APP_SELFTEST_AXIS, 1001);
    motor_t *m = &motors[APP_SELFTEST_AXIS];
    assert(!SelfTest_Update(&st, motors, &path, 1001, &cmd));
    assert(st.state == SELFTEST_ENABLE);
    assert(SelfTest_Update(&st, motors, &path, 1002, &cmd) && cmd.kind == MOTOR_CMD_ENABLE);
    SelfTest_CommandResult(&st, true, 1002);
    assert(!SelfTest_Update(&st, motors, &path, 1003, &cmd) && st.state == SELFTEST_ENABLE);
    /* Old enabled flags cannot confirm the newly accepted enable command. */
    m->flags_ms = 1004;
    assert(!SelfTest_Update(&st, motors, &path, 1004, &cmd) && st.state == SELFTEST_ZERO);
    assert(SelfTest_Update(&st, motors, &path, 1005, &cmd) && cmd.kind == MOTOR_CMD_ORIGIN);
    SelfTest_CommandResult(&st, true, 1005);
    m->position_valid = false;
    assert(!SelfTest_Update(&st, motors, &path, 1006, &cmd) && st.state == SELFTEST_ZERO);
    m->position_valid = true;
    m->position_ms = m->flags_ms = 1007;
    m->position = 65536;
    assert(!SelfTest_Update(&st, motors, &path, 1007, &cmd) && st.state == SELFTEST_ZERO);
    m->position = 0;
    assert(!SelfTest_Update(&st, motors, &path, 1008, &cmd) && st.state == SELFTEST_PATH);
    assert(SelfTest_Update(&st, motors, &path, 1009, &cmd));
    assert(cmd.kind == MOTOR_CMD_PATH && cmd.path.cycles == 1 && Motion_Validate(&cmd.path));
    assert(cmd.path.points[0].target[APP_SELFTEST_AXIS] == APP_SELFTEST_TARGET);
    assert(cmd.path.points[1].target[APP_SELFTEST_AXIS] == 0);
    SelfTest_CommandResult(&st, true, 1009);
    path.status = PATH_RUNNING;
    assert(!SelfTest_Update(&st, motors, &path, 1010, &cmd));
    path.status = PATH_DONE;
    assert(!SelfTest_Update(&st, motors, &path, 1011, &cmd));
    assert(st.state == SELFTEST_PAUSE && st.rounds == 1);
    ready(APP_SELFTEST_AXIS, 1510);
    assert(!SelfTest_Update(&st, motors, &path, 1510, &cmd) && st.state == SELFTEST_PAUSE);
    assert(!SelfTest_Update(&st, motors, &path, 1511, &cmd) && st.state == SELFTEST_PATH);
    assert(SelfTest_Update(&st, motors, &path, 1512, &cmd));
    SelfTest_CommandResult(&st, false, 1512);
    assert(st.state == SELFTEST_FAILED && !SelfTest_Update(&st, motors, &path, 1513, &cmd));
    /* Stop in any active phase is latched: no autonomous restart. */
    for (int phase = SELFTEST_WAIT_ONLINE; phase <= SELFTEST_PAUSE; phase++)
    {
        st.state = (selftest_state_t)phase;
        SelfTest_Cancel(&st);
        assert(st.state == SELFTEST_STOPPED && !SelfTest_Update(&st, motors, &path, 2000, &cmd));
        SelfTest_CommandResult(&st, true, 2000);
        assert(st.state == SELFTEST_STOPPED);
    }
    init();
    SelfTest_Init(&st, UINT32_MAX - 2000);
    assert(!SelfTest_Update(&st, motors, &path, 3000, &cmd) && st.state == SELFTEST_FAILED);
    SelfTest_Init(&st, 4000);
    ready(APP_SELFTEST_AXIS, 4000);
    motors[APP_SELFTEST_AXIS].fault = MOTOR_COMM_FAULT;
    assert(!SelfTest_Update(&st, motors, &path, 4001, &cmd) && st.state == SELFTEST_FAILED);
    puts("PASS selftest: fresh enable/zero confirmation, return-to-zero round, repeat delay, "
         "rejection, stop, timeout/wrap, fault");
#else
    assert(st.state == SELFTEST_DISABLED);
    assert(!SelfTest_Update(&st, motors, &path, 1001, &cmd));
    SelfTest_CommandResult(&st, false, 1001);
    SelfTest_Cancel(&st);
    assert(st.state == SELFTEST_DISABLED);
    puts("PASS selftest disabled: no commands, no auto-start");
#endif
}

/* Exercise complete actions through the real path executor with simulated feedback. */
static void action_ready(void)
{
    init();
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        ready(i, 1000);
    motors[MOTOR_X].position = 10 * 65536;
    motors[MOTOR_Z].position = 10 * 65536;
    motors[MOTOR_YAW].position = 3641; /* about 20 degrees, deliberately nonzero */
}
static void actions(void)
{
    for (unsigned mode = 0; mode < ACTION_MODE_COUNT; mode++)
    {
        action_ready();
        motor_action_t a = {0};
        motion_state_t p = {0};
        assert(Action_Start(&a, (motor_action_mode_t)mode, &p, motors, 1000));
        assert(!Action_Start(&a, ACTION_PRESS, &p, motors, 1000));
        unsigned moves[AXIS_COUNT] = {0};
        motor_action_stage_t stages[8];
        unsigned count = 0;
        stages[count++] = a.stage;
        for (uint32_t now = 1002; now < 3000 && a.status == ACTION_RUNNING; now += 2)
        {
            for (unsigned i = 0; i < AXIS_COUNT; i++)
            {
                motors[i].flags_ms = motors[i].position_ms = now;
                if (motors[i].pending == EMM_MOVE)
                {
                    moves[i]++;
                    float target = p.path.points[p.point].target[i];
                    motors[i].position =
                        (int64_t)llroundf(target / g_axis_config[i].units_per_motor_rev * 65536.0f);
                    ack(i, EMM_MOVE, 2, now);
                }
            }
            Motion_Update(&p, motors, now);
            motor_action_stage_t previous = a.stage;
            Action_Update(&a, &p, motors, now);
            if (a.stage != previous)
            {
                if (previous == ACTION_STAGE_PRESS)
                    assert(now >= 1304); /* dwell starts after fresh arrival at 1004 */
                assert(count < 8);
                stages[count++] = a.stage;
            }
        }
        assert(a.status == ACTION_DONE && p.status == PATH_DONE);
        assert(moves[MOTOR_Z] == 2);
        assert(moves[MOTOR_X] == (mode == ACTION_SLIDE ? 2U : 0U));
        assert(moves[MOTOR_YAW] == (mode == ACTION_RUB ? 7U : 0U));
        assert(stages[0] == ACTION_STAGE_PRESS && stages[count - 1] == ACTION_STAGE_FINISHED);
        if (mode == ACTION_PRESS)
            assert(count == 3 && stages[1] == ACTION_STAGE_RETRACT);
        if (mode == ACTION_RUB)
            assert(count == 5 && stages[1] == ACTION_STAGE_RUB &&
                   stages[2] == ACTION_STAGE_YAW_RETURN && stages[3] == ACTION_STAGE_RETRACT);
        if (mode == ACTION_SLIDE)
            assert(count == 5 && stages[1] == ACTION_STAGE_SLIDE &&
                   stages[2] == ACTION_STAGE_RETRACT && stages[3] == ACTION_STAGE_X_RETURN);
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            if (a.axis_mask & (1U << i))
                assert(fabsf(Axis_Position(&g_axis_config[i], motors[i].position) - a.start[i]) <
                       0.01f);
    }
    action_ready();
    motor_action_t a = {0};
    motion_state_t p = {0};
    motors[MOTOR_X].position = 45 * 65536;
    assert(!Action_Start(&a, ACTION_SLIDE, &p, motors, 1000)); /* later X target exceeds limit */
    assert(a.status == ACTION_IDLE && p.status != PATH_RUNNING && !motors[MOTOR_Z].pending);
    motors[MOTOR_YAW].position = 32000; /* later positive yaw exceeds 180 degrees */
    assert(!Action_Start(&a, ACTION_RUB, &p, motors, 1000));
    motors[MOTOR_YAW].position = -32000;
    assert(!Action_Start(&a, ACTION_RUB, &p, motors, 1000));
    assert(!Action_Start(&a, (motor_action_mode_t)99, &p, motors, 1000));
    motors[MOTOR_Z].origin_valid = false;
    assert(!Action_Start(&a, ACTION_PRESS, &p, motors, 1000));
    ready(MOTOR_Z, 1000);
    motors[MOTOR_X].fault = MOTOR_COMM_FAULT;
    assert(Action_Start(&a, ACTION_PRESS, &p, motors, 1000)); /* unrelated X does not block Z */
    Motion_Abort(&p, motors, false);
    Action_Update(&a, &p, motors, 1001);
    assert(a.status == ACTION_ABORTED);
    Action_Update(&a, &p, motors, 1002);
    assert(a.status == ACTION_ABORTED);
    action_ready();
    a = (motor_action_t){0};
    p = (motion_state_t){0};
    assert(Action_Start(&a, ACTION_SLIDE, &p, motors, 1000));
    p.status = PATH_DONE;
    Action_Update(&a, &p, motors, 1001);
    assert(a.stage == ACTION_STAGE_SLIDE);
    motors[MOTOR_Z].flags_ms = 0; /* stationary pressed axis becomes stale */
    Action_Update(&a, &p, motors, 1002);
    assert(a.status == ACTION_FAILED && p.status == PATH_FAILED);
    assert(motors[MOTOR_Z].stop_requested && motors[MOTOR_X].stop_requested);
    Action_Update(&a, &p, motors, 1003);
    assert(a.stage == ACTION_STAGE_SLIDE);
    puts("PASS actions: complete press/rub/slide, captured origins, ordered retract, preflight "
         "limits, conflicts, stop, stationary-axis fault");
}

static void startup_tests(void)
{
    init();
    motor_startup_t s;
    motor_action_t a = {0};
    motor_command_t cmd;
    uint32_t now = 1000;
    Startup_Init(&s, now);
#if APP_MOTOR_AUTOSTART
    assert(s.state == STARTUP_WAIT_ONLINE);
    assert(!Startup_Update(&s, motors, &a, now, &cmd));
    for (unsigned i = 0; i < AXIS_COUNT; i++)
        if (s.axis_mask & (1U << i))
            ready(i, now);
    assert(!Startup_Update(&s, motors, &a, now, &cmd) && s.state == STARTUP_ENABLE);
    unsigned prepared = 0;
    while (s.state == STARTUP_ENABLE)
    {
        unsigned axis = s.axis;
        assert(Startup_Update(&s, motors, &a, now, &cmd) && cmd.kind == MOTOR_CMD_ENABLE &&
               cmd.axis == axis && cmd.enable);
        Startup_CommandResult(&s, true, now);
        assert(!Startup_Update(&s, motors, &a, now, &cmd) && s.state == STARTUP_ENABLE);
        motors[axis].flags_ms = ++now;
        assert(!Startup_Update(&s, motors, &a, now, &cmd) && s.state == STARTUP_ZERO);
        assert(Startup_Update(&s, motors, &a, now, &cmd) && cmd.kind == MOTOR_CMD_ORIGIN &&
               cmd.axis == axis);
        Startup_CommandResult(&s, true, now);
        motors[axis].position_valid = false;
        assert(!Startup_Update(&s, motors, &a, ++now, &cmd) && s.state == STARTUP_ZERO);
        ready(axis, ++now);
        motors[axis].position = 65536;
        assert(!Startup_Update(&s, motors, &a, now, &cmd) && s.state == STARTUP_ZERO);
        motors[axis].position = 0;
        assert(!Startup_Update(&s, motors, &a, ++now, &cmd));
        prepared++;
    }
    assert(prepared == (APP_AUTOSTART_MODE == ACTION_PRESS ? 1U : 2U));
    assert(s.state == STARTUP_ACTION);
    assert(Startup_Update(&s, motors, &a, now, &cmd) && cmd.kind == MOTOR_CMD_ACTION &&
           cmd.action_mode == APP_AUTOSTART_MODE);
    Startup_CommandResult(&s, true, now);
    a.status = ACTION_RUNNING;
    assert(!Startup_Update(&s, motors, &a, ++now, &cmd) && s.state == STARTUP_ACTION);
    a.status = ACTION_DONE;
    assert(!Startup_Update(&s, motors, &a, ++now, &cmd) && s.state == STARTUP_DONE);
    assert(!Startup_Update(&s, motors, &a, now + 100000, &cmd) && s.state == STARTUP_DONE);
    for (int phase = STARTUP_WAIT_ONLINE; phase <= STARTUP_ACTION; phase++)
    {
        s.state = (motor_startup_state_t)phase;
        Startup_Cancel(&s);
        Startup_CommandResult(&s, true, now);
        assert(!Startup_Update(&s, motors, &a, now, &cmd) && s.state == STARTUP_STOPPED);
    }
    Startup_Init(&s, UINT32_MAX - 2000);
    assert(!Startup_Update(&s, motors, &a, 4000, &cmd) && s.state == STARTUP_FAILED);
    Startup_Init(&s, now);
    Startup_CommandResult(&s, false, now);
    assert(s.state == STARTUP_FAILED && !Startup_Update(&s, motors, &a, now, &cmd));
    Startup_Init(&s, now);
    motors[s.axis].fault = MOTOR_COMM_FAULT;
    assert(!Startup_Update(&s, motors, &a, now, &cmd) && s.state == STARTUP_FAILED);
    puts("PASS startup: selected axes, fresh enable/zero gates, one action, no repeat, "
         "cancellation, rejection, timeout/wrap, fault");
#else
    assert(s.state == STARTUP_DISABLED);
    assert(!Startup_Update(&s, motors, &a, now, &cmd));
    Startup_CommandResult(&s, true, now);
    Startup_Cancel(&s);
    assert(s.state == STARTUP_DISABLED);
    puts("PASS startup disabled: no automatic commands");
#endif
}

/* Polling transactions must not reset an already confirmed arrival dwell. */
static void dwell_with_polling(void)
{
    init();
    ready(MOTOR_Z, 1000);
    motion_state_t path = {0};
    motor_action_t action = {0};
    assert(Action_Start(&action, ACTION_PRESS, &path, motors, 1000));
    Motion_Update(&path, motors, 1001);
    ack(MOTOR_Z, EMM_MOVE, 2, 1002);
    const float target = g_action_config[ACTION_PRESS].z_press_mm;
    motors[MOTOR_Z].position = (int64_t)llroundf(target * 65536);
    for (uint32_t now = 1003; now <= 1303; now++)
    {
        motor_t *m = &motors[MOTOR_Z];
        /* Query replies remain fresh; transactions span multiple task iterations. */
        if (now % 20 == 0)
            m->position_ms = m->flags_ms = now;
        if (now == 1003)
            m->position_ms = m->flags_ms = now;
        m->pending = (now % 20 < 10) ? EMM_FLAGS : 0;
        Motion_Update(&path, motors, now);
        if (now < 1303)
            assert(path.status == PATH_RUNNING);
    }
    assert(path.status == PATH_DONE);
    Action_Update(&action, &path, motors, 1303);
    assert(action.stage == ACTION_STAGE_RETRACT);
    motor_t *m = &motors[MOTOR_Z];
    const uint8_t queries[] = {EMM_POSITION, EMM_FLAGS, EMM_SPEED};
    for (unsigned i = 0; i < 3; i++)
    {
        m->pending = queries[i];
        assert(Axis_AtTarget(m, MOTOR_Z, target, 1001, 1303));
    }
    m->pending = EMM_MOVE;
    assert(!Axis_AtTarget(m, MOTOR_Z, target, 1001, 1303));
    m->pending = EMM_FLAGS;
    m->position_ms = 1000;
    assert(!Axis_AtTarget(m, MOTOR_Z, target, 1001, 1303));
    puts("PASS dwell: background queries preserve 300ms hold, controls/stale feedback still block "
         "arrival");
}

static remote_reply_t last_reply;
static unsigned reply_count;
static void receive_reply(remote_reply_t q)
{
    last_reply = q;
    reply_count++;
}
static void remote_tests(void)
{
    uint8_t frame[32], payload[6] = {1, 0, 0, 0, RC_SELECT, ACTION_SLIDE};
    size_t len = RC_Build(frame, RC_REQUEST, payload, 6);
    assert(len == 11 && frame[0] == 0xAA && frame[1] == 0x55);
    remote_parser_t parser = {0};
    unsigned frames = 0;
    for (unsigned i = 0; i < len; i++)
        frames += RC_Parse(&parser, frame[i], 100 + i);
    assert(frames == 1 && RC_Read32(parser.bytes + 4) == 1);
    frame[len - 1] ^= 1;
    for (unsigned i = 0; i < len; i++)
        assert(!RC_Parse(&parser, frame[i], 200 + i));
    frame[len - 1] ^= 1;
    assert(!RC_Parse(&parser, 0xAA, 300));
    assert(!RC_Parse(&parser, 0x55, 301));
    for (unsigned i = 0; i < len; i++)
        frames += RC_Parse(&parser, frame[i], 400 + i);
    assert(frames == 2); /* incomplete frame discarded after a gap */
    assert(!RC_Parse(&parser, 0xAA, 500));
    assert(!RC_Parse(&parser, 0x55, 501));
    assert(!RC_Parse(&parser, RC_REQUEST, 502));
    assert(!RC_Parse(&parser, 255, 503));
    for (unsigned i = 0; i < len; i++)
        frames += RC_Parse(&parser, frame[i], 510 + i);
    assert(frames == 3);
    for (unsigned mode = 0; mode < 3; mode++)
    {
        init();
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            ready(i, 1000);
        motors[MOTOR_Z].origin_valid = false;
        remote_control_t r;
        motion_state_t p = {0};
        motor_action_t a = {0};
        Remote_Init(&r, receive_reply);
        Remote_Handle(&r, (remote_request_t){1, RC_START, 0}, motors, &p, &a, 1000);
        assert(last_reply.result == RC_NOT_READY);
        Remote_Handle(&r, (remote_request_t){2, RC_SELECT, (uint8_t)mode}, motors, &p, &a, 1000);
        assert(last_reply.result == RC_COMPLETE && r.status.state == RC_IDLE);
        Remote_Handle(&r, (remote_request_t){3, RC_ARM, 0}, motors, &p, &a, 1000);
        assert(last_reply.result == RC_ACCEPTED);
        unsigned zero_count = 0;
        uint32_t now;
        for (now = 1002; now < 2000 && r.status.state == RC_PREPARING; now += 2)
        {
            for (unsigned i = 0; i < AXIS_COUNT; i++)
            {
                if (motors[i].pending == EMM_ENABLE)
                    ack(i, EMM_ENABLE, 2, now);
                if (motors[i].pending == EMM_ZERO)
                {
                    ack(i, EMM_ZERO, 2, now);
                    zero_count++;
                }
                motors[i].position_valid = true;
                motors[i].position = 0;
                motors[i].position_ms = motors[i].flags_ms = now;
            }
            Remote_Update(&r, motors, &p, &a, now, true);
        }
        assert(r.status.state == RC_ARMED && last_reply.result == RC_COMPLETE && zero_count == 1);
        /* Completed ARM replay must not enable/zero again. */
        Remote_Handle(&r, (remote_request_t){3, RC_ARM, 0}, motors, &p, &a, now);
        assert(r.status.state == RC_ARMED && last_reply.result == RC_COMPLETE);
        Remote_Handle(&r, (remote_request_t){4, RC_START, 0}, motors, &p, &a, now);
        assert(r.status.state == RC_RUNNING && last_reply.result == RC_ACCEPTED);
        unsigned replies_before = reply_count;
        Remote_Handle(&r, (remote_request_t){4, RC_START, 0}, motors, &p, &a, now);
        assert(reply_count == replies_before + 1 && last_reply.result == RC_ACCEPTED);
        Remote_Handle(&r, (remote_request_t){5, RC_SELECT, 0}, motors, &p, &a, now);
        assert(last_reply.result == RC_BUSY);
        uint32_t end = now + 2000;
        for (now += 2; now < end && r.status.state == RC_RUNNING; now += 2)
        {
            for (unsigned i = 0; i < AXIS_COUNT; i++)
            {
                if (motors[i].pending == EMM_MOVE)
                {
                    motors[i].position =
                        (int64_t)llroundf(p.path.points[p.point].target[i] /
                                          g_axis_config[i].units_per_motor_rev * 65536);
                    ack(i, EMM_MOVE, 2, now);
                }
                motors[i].position_ms = motors[i].flags_ms = now;
            }
            Motion_Update(&p, motors, now);
            Action_Update(&a, &p, motors, now);
            Remote_Update(&r, motors, &p, &a, now, true);
        }
        assert(r.status.state == RC_DONE && last_reply.seq == 4 &&
               last_reply.result == RC_COMPLETE);
        Remote_Handle(&r, (remote_request_t){4, RC_START, 0}, motors, &p, &a, now);
        assert(r.status.state == RC_DONE && last_reply.result == RC_COMPLETE);
        Remote_Handle(&r, (remote_request_t){4, RC_ARM, 0}, motors, &p, &a, now);
        assert(last_reply.result == RC_INVALID);
        Remote_Handle(&r, (remote_request_t){6, RC_ARM, 0}, motors, &p, &a, now);
        Remote_Handle(&r, (remote_request_t){7, RC_STOP, 0}, motors, &p, &a, now);
        assert(r.status.state == RC_STOPPING && last_reply.result == RC_ACCEPTED);
        for (unsigned i = 0; i < AXIS_COUNT; i++)
        {
            motors[i].stop_requested = false;
            motors[i].pending = 0;
            motors[i].port->tx_busy = false;
        }
        Remote_Update(&r, motors, &p, &a, now, true);
        assert(r.status.state == RC_IDLE && last_reply.result == RC_COMPLETE);
        Remote_Handle(&r, (remote_request_t){8, RC_ARM, 0}, motors, &p, &a, now);
        Remote_Update(&r, motors, &p, &a, now, false);
        assert(r.status.state == RC_STOPPING && last_reply.result == RC_FAULT);
    }
    /* Re-ARM preserves a valid nonzero coordinate; it never resets it to zero. */
    init();
    ready(MOTOR_Z, 1000);
    motors[MOTOR_Z].position = 10 * 65536;
    motor_startup_t prep;
    motor_command_t cmd;
    motor_action_t action = {0};
    Startup_Prepare(&prep, ACTION_PRESS, 1000);
    assert(!Startup_Update(&prep, motors, &action, 1000, &cmd));
    assert(Startup_Update(&prep, motors, &action, 1001, &cmd) && cmd.kind == MOTOR_CMD_ENABLE);
    Startup_CommandResult(&prep, true, 1001);
    motors[MOTOR_Z].flags_ms = 1002;
    assert(!Startup_Update(&prep, motors, &action, 1002, &cmd) && prep.state == STARTUP_ZERO);
    assert(!Startup_Update(&prep, motors, &action, 1003, &cmd) && prep.state == STARTUP_DONE);
    assert(motors[MOTOR_Z].position == 10 * 65536);
    puts("PASS remote: CRC/framing/recovery, mode/ARM/START all modes, origin preservation, "
         "duplicates, conflicts, STOP, link loss");
}
int main(void)
{
    remote_tests();
    dwell_with_polling();
    startup_tests();
    selftests();
    protocol();
    conversions();
    transport();
    transactions();
    paths();
    actions();
    buzzer();
    puts("ALL TESTS PASSED");
    return 0;
}
