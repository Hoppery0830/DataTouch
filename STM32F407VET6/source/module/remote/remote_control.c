/**
 * @file remote_control.c
 * @brief 由 MotorTask 管理远程会话、采图门控、运动和结果缓存。
 */
#include "remote_control.h"
#include <string.h>
#define RESET_TIMEOUT_MS 5000U
#define RESET_STABLE_MS 200U
#define RESET_RETRY_MS 200U
#include <math.h>
#include "touch_config.h"
void Remote_SetVision(remote_control_t *r, const remote_vision_ops_t *ops)
{
    r->vision = ops;
}
static void result(remote_control_t *r, uint32_t seq, uint8_t value)
{
    for (unsigned i = 0; i < 16; i++)
        if (r->cache[i].valid && r->cache[i].request.seq == seq)
            r->cache[i].result = value;
    if (r->emit)
        r->emit((remote_reply_t){ seq, value });
}
static void finish(remote_control_t *r, uint8_t value)
{
    if (r->active)
        result(r, r->active_seq, value);
    r->active = false;
}
void Remote_Init(remote_control_t *r, remote_emit_t emit)
{
    memset(r, 0, sizeof(*r));
    r->status.mode = 255;
    r->emit = emit;
}
static void stop(remote_control_t *r, motor_t m[AXIS_COUNT], motion_state_t *p, motor_action_t *a,
                 uint32_t now, uint8_t target)
{
    if (r->vision)
        r->vision->cancel();
    r->capture_started = false;
    Startup_Cancel(&r->preparation);
    Action_Cancel(a);
    Motion_Abort(p, m, false);
    r->status.state = RC_STOPPING;
    r->stop_target = target;
    r->stop_ms = now;
    r->resetting = false;
}
void Remote_Cancel(remote_control_t *r, motor_t m[AXIS_COUNT], motion_state_t *p, motor_action_t *a,
                   uint32_t now)
{
    finish(r, RC_CANCELLED);
    stop(r, m, p, a, now, RC_IDLE);
}
void Remote_Handle(remote_control_t *r, remote_request_t q, motor_t m[AXIS_COUNT],
                   motion_state_t *p, motor_action_t *a, uint32_t now)
{
    for (unsigned i = 0; i < 16; i++)
        if (r->cache[i].valid && r->cache[i].request.seq == q.seq) {
            uint8_t v = (r->cache[i].request.op == q.op && r->cache[i].request.mode == q.mode) ? r->cache[i].result : RC_INVALID;
            if (r->emit)
                r->emit((remote_reply_t){ q.seq, v });
            return;
        }
    unsigned slot = r->next++ % 16;
    r->cache[slot].valid = true;
    r->cache[slot].request = q;
    r->cache[slot].result = RC_INVALID;
    if (q.op > RC_RESET) {
        result(r, q.seq, RC_INVALID);
        return;
    }
    if (q.op == RC_STOP || q.op == RC_RESET) {
        finish(r, RC_CANCELLED);
        r->active = true;
        r->active_seq = q.seq;
        stop(r, m, p, a, now, RC_IDLE);
        if (q.op == RC_RESET)
        {
            r->resetting = true;
            r->reset_stable_mask = r->reset_clear_mask = 0;
            for (unsigned i = 0; i < AXIS_COUNT; i++)
            {
                r->reset_acks[i] = m[i].stop_acks;
                r->reset_retry[i] = now;
                m[i].origin_valid = false;
            }
        }

        result(r, q.seq, RC_ACCEPTED);
        return;
    }
    if (r->active || r->status.state == RC_STOPPING || p->status == PATH_RUNNING) {
        result(r, q.seq, RC_BUSY);
        return;
    }
    if (q.op == RC_SELECT) {
        if (q.mode >= ACTION_MODE_COUNT) {
            result(r, q.seq, RC_INVALID);
            return;
        }
        r->status.mode = q.mode;
        r->selected = true;
        r->status.state = RC_IDLE;
        r->mask = (1U << MOTOR_Z) | (q.mode == ACTION_RUB ? (1U << MOTOR_YAW) : q.mode == ACTION_SLIDE ? (1U << MOTOR_X) :
                                                                                                         0);
        result(r, q.seq, RC_COMPLETE);
        return;
    }
    if (!r->selected) {
        result(r, q.seq, RC_NOT_READY);
        return;
    }
    if (q.op == RC_ARM) {
        Startup_Prepare(&r->preparation, (motor_action_mode_t)r->status.mode, now);
        /* Every axis must remain stationary during exposure, including axes unused by the action.
         */
        r->preparation.axis_mask = 7;
        r->preparation.axis = 0;
        r->mask = 7;
        r->status.state = RC_PREPARING;
    } else {
        if (r->status.state != RC_ARMED) {
            result(r, q.seq, RC_NOT_READY);
            return;
        }
        if (r->vision && r->vision->available && !r->vision->available()) {
            result(r, q.seq, RC_NOT_READY);
            return;
        }
        if (!r->vision || !r->vision->stationary(m, now)) {
            result(r, q.seq, RC_NOT_STATIONARY);
            return;
        }
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            r->capture_position[i] = Axis_Position(&g_axis_config[i], m[i].position);
        r->capture_wait_ms = now;
        r->capture_started = false;
        r->status.state = RC_IMAGING;
        r->status.stage = 8;
    }
    r->active = true;
    r->active_seq = q.seq;
    result(r, q.seq, RC_ACCEPTED);
}
static void reset_update(remote_control_t *r, motor_t m[AXIS_COUNT], uint32_t now,
                         bool link_alive)
{
    if (!link_alive || now - r->stop_ms >= RESET_TIMEOUT_MS)
    {
        r->resetting = false;
        r->status.state = RC_ERROR;
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            Motor_RequestStop(&m[i]);
        finish(r, RC_FAULT);
        return;
    }
    for (unsigned i = 0; i < AXIS_COUNT; i++)
    {
        motor_t *motor = &m[i];
        uint8_t bit = (uint8_t)(1U << i);
        bool stopped = motor->stop_acks != r->reset_acks[i] &&
                       !motor->stop_failed && !motor->stop_requested;
        if (!stopped && !motor->pending && !motor->port->tx_busy &&
            !motor->stop_requested && !motor->port->broken &&
            now - r->reset_retry[i] >= RESET_RETRY_MS)
        {
            Motor_RequestStop(motor);
            r->reset_retry[i] = now;
        }
        bool feedback = stopped && !motor->port->broken &&
                        motor->position_valid && motor->flags_valid &&
                        !(motor->flags & 0x0CU) && motor->mode == MOTOR_HOLD &&
                        (int32_t)(motor->position_ms - motor->stop_ack_ms) > 0 &&
                        (int32_t)(motor->flags_ms - motor->stop_ack_ms) > 0 &&
                        now - motor->position_ms < APP_MOTOR_STALE_MS &&
                        now - motor->flags_ms < APP_MOTOR_STALE_MS;
        if (!feedback)
        {
            r->reset_stable_mask &= (uint8_t)~bit;
            r->reset_clear_mask &= (uint8_t)~bit;
            continue;
        }
        if (!(r->reset_stable_mask & bit))
        {
            r->reset_stable_mask |= bit;
            r->reset_stable[i] = now;
        }
        if (motor->fault)
            r->reset_clear_mask &= (uint8_t)~bit;
        if (now - r->reset_stable[i] >= RESET_STABLE_MS &&
            !motor->port->tx_busy && Motor_ClearFault(motor, now))
            r->reset_clear_mask |= bit;
    }
    if (r->reset_clear_mask == (1U << AXIS_COUNT) - 1U &&
        (!r->vision || !r->vision->light_off || r->vision->light_off()))
    {
        r->resetting = false;
        r->status.state = RC_IDLE;
        r->status.fault = 0;
        r->status.online = (1U << AXIS_COUNT) - 1U;
        finish(r, RC_COMPLETE);
    }
}

void Remote_Update(remote_control_t *r, motor_t m[AXIS_COUNT], motion_state_t *p, motor_action_t *a,
                   uint32_t now, bool link_alive)
{
    r->status.online = r->status.fault = 0;
    if (r->status.state != RC_IMAGING)
        r->status.stage = (uint8_t)a->stage;
    if (r->status.state == RC_IDLE || r->status.state == RC_ARMED ||
        r->status.state == RC_PREPARING)
        r->status.stage = 0;
    for (unsigned i = 0; i < AXIS_COUNT; i++) {
        if (Motor_Healthy(&m[i], now))
            r->status.online |= 1U << i;
        if (m[i].fault || m[i].stop_failed)
            r->status.fault |= 1U << i;
    }
    if (r->resetting) { reset_update(r, m, now, link_alive); return; }
    if (!link_alive && (r->status.state == RC_PREPARING || r->status.state == RC_RUNNING ||
                        r->status.state == RC_ARMED || r->status.state == RC_IMAGING)) {
        finish(r, RC_FAULT);
        stop(r, m, p, a, now, RC_ERROR);
    }
    if (r->status.state == RC_PREPARING) {
        motor_command_t cmd;
        if (Startup_Update(&r->preparation, m, a, now, &cmd)) {
            motor_t *motor = &m[cmd.axis];
            /* Wait for background transactions without claiming dispatch success. */
            if (!motor->pending && !motor->port->tx_busy && !motor->stop_requested) {
                bool ok = cmd.kind == MOTOR_CMD_ENABLE ? Motor_Enable(motor, true, now) : Motor_Zero(motor, now);
                Startup_CommandResult(&r->preparation, ok, now);
            }
        }
        if (r->preparation.state == STARTUP_DONE) {
            bool all_ready = true;
            for (unsigned i = 0; i < AXIS_COUNT; i++)
                if (r->mask & (1U << i))
                    if (!Motor_Healthy(&m[i], now) || !m[i].origin_valid ||
                        !m[i].enable_requested || !(m[i].flags & 1U))
                        all_ready = false;
            if (all_ready) {
                r->status.state = RC_ARMED;
                finish(r, RC_COMPLETE);
            } else {
                finish(r, RC_FAULT);
                stop(r, m, p, a, now, RC_ERROR);
            }
        } else if (r->preparation.state == STARTUP_FAILED) {
            finish(r, RC_FAULT);
            stop(r, m, p, a, now, RC_ERROR);
        }
    } else if (r->status.state == RC_IMAGING) {
        uint8_t failure = 0;
        bool stationary = r->vision->stationary(m, now);
        for (unsigned i = 0; i < AXIS_COUNT; i++) {
            float delta = Axis_Position(&g_axis_config[i], m[i].position) - r->capture_position[i];
            if (!isfinite(delta) || fabsf(delta) > g_axis_config[i].tolerance)
                stationary = false;
        }
        if (!stationary)
            failure = RC_NOT_STATIONARY;
        else if (now - r->capture_wait_ms >= TOUCH_CAPTURE_TOTAL_MS + TOUCH_STILL_MS + 1000U)
            failure = RC_CAPTURE_TIMEOUT;
        else if (!r->capture_started) {
            bool fresh = true;
            for (unsigned i = 0; i < AXIS_COUNT; i++)
                if ((int32_t)(m[i].position_ms - r->capture_wait_ms) <= 0 ||
                    (int32_t)(m[i].flags_ms - r->capture_wait_ms) <= 0)
                    fresh = false;
            if (now - r->capture_wait_ms >= TOUCH_STILL_MS && fresh) {
                if (r->vision->begin(r->active_seq, now)) {
                    r->capture_started = true;
                    r->status.stage = 9;
                } else
                    failure = RC_NOT_READY;
            }
        } else {
            capture_sequence_t c;
            r->vision->status(r->active_seq, &c);
            r->status.stage = c.state == CAP_WAIT ? 10 : 9;
            if (c.state == CAP_READY && !c.light) {
                r->status.stage = 11;
                if (Action_Start(a, (motor_action_mode_t)r->status.mode, p, m, now))
                    r->status.state = RC_RUNNING;
                else
                    failure = RC_FAULT;
            } else if (c.state == CAP_FAILED) {
                failure = c.error == CAP_TIMEOUT ? RC_CAPTURE_TIMEOUT : c.error == CAP_LIGHT_ERROR ? RC_LIGHT_FAILED :
                                                                                                     RC_CAPTURE_FAILED;
            }
        }
        if (failure) {
            finish(r, failure);
            stop(r, m, p, a, now, RC_ERROR);
        }
    } else if (r->status.state == RC_RUNNING) {
        if (a->status == ACTION_DONE) {
            r->status.state = RC_DONE;
            finish(r, RC_COMPLETE);
        } else if (a->status == ACTION_FAILED || a->status == ACTION_ABORTED) {
            r->status.state = RC_ERROR;
            finish(r, RC_FAULT);
        }
    } else if (r->status.state == RC_ARMED) {
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            if ((r->mask & (1U << i)) &&
                (!m[i].enable_requested || !(m[i].flags & 1U) || !m[i].origin_valid))
                r->status.state = RC_ERROR;
        if ((r->status.online & r->mask) != r->mask || (r->status.fault & r->mask))
            r->status.state = RC_ERROR;
    } else if (r->status.state == RC_STOPPING) {
        bool stopped = true, failed = false;
        /* Nonparticipating offline axes cannot confirm stop; only selected axes gate completion. */
        for (unsigned i = 0; i < AXIS_COUNT; i++)
            if (r->mask & (1U << i)) {
                if (m[i].stop_requested || m[i].pending || m[i].port->tx_busy)
                    stopped = false;
                if (m[i].stop_failed || m[i].fault)
                    failed = true;
            }
        if (stopped || now - r->stop_ms >= 1000) {
            r->status.state = (failed || !stopped) ? RC_ERROR : r->stop_target;
            finish(r, r->status.state == RC_ERROR ? RC_FAULT : RC_COMPLETE);
        }
    }
}
