#include "Copter.h"

// Code to detect a crash main ArduCopter code
#define CRASH_CHECK_TRIGGER_SEC         2       // 2 seconds inverted indicates a crash
#define CRASH_CHECK_ANGLE_DEVIATION_DEG 30.0f   // 30 degrees beyond angle max is signal we are inverted
#define CRASH_CHECK_ACCEL_MAX           3.0f    // vehicle must be accelerating less than 3m/s/s to be considered crashed

// crash_check - disarms motors if a crash has been detected
// crashes are detected by the vehicle being more than 20 degrees beyond it's angle limits continuously for more than 1 second
// called at MAIN_LOOP_RATE
void Copter::crash_check()
{
    static uint16_t crash_counter;  // number of iterations vehicle may have been crashed

    // return immediately if disarmed, or crash checking disabled
    if (!motors->armed() || ap.land_complete || g.fs_crash_check == 0) {
        crash_counter = 0;
        return;
    }

    // return immediately if we are not in an angle stabilize flight mode or we are flipping
    if (control_mode == ACRO || control_mode == FLIP) {
        crash_counter = 0;
        return;
    }

    // vehicle not crashed if 1hz filtered acceleration is more than 3m/s (1G on Z-axis has been subtracted)
    if (land_accel_ef_filter.get().length() >= CRASH_CHECK_ACCEL_MAX) {
        crash_counter = 0;
        return;
    }

    // check for angle error over 30 degrees
    const float angle_error = attitude_control->get_att_error_angle_deg();
    if (angle_error <= CRASH_CHECK_ANGLE_DEVIATION_DEG) {
        crash_counter = 0;
        return;
    }

    // we may be crashing
    crash_counter++;

    // check if crashing for 2 seconds
    if (crash_counter >= (CRASH_CHECK_TRIGGER_SEC * scheduler.get_loop_rate_hz())) {
        // log an error in the dataflash
        Log_Write_Error(ERROR_SUBSYSTEM_CRASH_CHECK, ERROR_CODE_CRASH_CHECK_CRASH);
        // send message to gcs
        gcs().send_text(MAV_SEVERITY_EMERGENCY,"Crash: Disarming");
        // disarm motors
        init_disarm_motors();
    }
}

#if PARACHUTE == ENABLED

// Code to detect a crash main ArduCopter code
#define PARACHUTE_CHECK_ANGLE_DEVIATION_CD  3000    // 30 degrees off from target indicates a loss of control

// parachute_check - disarms motors and triggers the parachute if serious loss of control has been detected
// vehicle is considered to have a "serious loss of control" by the vehicle being more than 30 degrees off from the target roll and pitch angles continuously for 1 second
// called at MAIN_LOOP_RATE
void Copter::parachute_check()
{
    static uint16_t control_loss_count;	// number of iterations we have been out of control
    static uint32_t last_gcs_sent = 0;
    static uint8_t last_gcs_type = 0;

    // exit immediately if parachute is not enabled
    if (!parachute.enabled()) {
        return;
    }

    // call update to give parachute a chance to move servo or relay back to off position
    parachute.update();

    // return immediately if motors are not armed or pilot's throttle is above zero
    if (!motors->armed()) {
        control_loss_count = 0;
        Log_Write_Data(100, control_loss_count);

        uint32_t now = AP_HAL::millis();
        if (now - last_gcs_sent > 500 || last_gcs_type != 1) {
            gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: motors not armed, reset counter");
            last_gcs_sent = now;
            last_gcs_type = 1;
        }
        return;
    }

    // return immediately if we are not in an angle stabilize flight mode or we are flipping
    if (control_mode == ACRO || control_mode == FLIP) {
        control_loss_count = 0;
        Log_Write_Data(100, control_loss_count);

        uint32_t now = AP_HAL::millis();
        if (now - last_gcs_sent > 500 || last_gcs_type != 2) {
            gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: acro or flip, reset counter");
            last_gcs_sent = now;
            last_gcs_type = 2;
        }
        return;
    }

    // ensure we are flying
    if (ap.land_complete) {
        control_loss_count = 0;
        Log_Write_Data(100, control_loss_count);

        uint32_t now = AP_HAL::millis();
        if (now - last_gcs_sent > 500 || last_gcs_type != 3) {
            gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: landed, reset counter");
            last_gcs_sent = now;
            last_gcs_type = 3;
        }
        return;
    }

    Log_Write_Data(101, baro_climbrate);
    Log_Write_Data(103, climb_rate);
    // ensure we are going down
    if (is_positive(baro_climbrate)) {
        // only reset count if both agree that we are climbing
        if (climb_rate > 0) {
            control_loss_count = 0;
            Log_Write_Data(100, control_loss_count);

            uint32_t now = AP_HAL::millis();
            if (now - last_gcs_sent > 500 || last_gcs_type != 4) {
                gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: climbing, reset counter");
                last_gcs_sent = now;
                last_gcs_type = 4;
            }
        } else {
            uint32_t now = AP_HAL::millis();
            if (now - last_gcs_sent > 500 || last_gcs_type != 8) {
                gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: AHRS/baro not agree");
                last_gcs_sent = now;
                last_gcs_type = 8;
            }
        }
        return;
    } else if (climb_rate > 0) {
        // baro and AHRS have different climb rate signal, don't reset, but don't increase
        uint32_t now = AP_HAL::millis();
        if (now - last_gcs_sent > 500 || last_gcs_type != 8) {
            gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: AHRS/baro not agree");
            last_gcs_sent = now;
            last_gcs_type = 8;
        }
        return;
    }

    // ensure the first control_loss event is from above the min altitude
    if (control_loss_count == 0 && parachute.alt_min() != 0 && (current_loc.alt < (int32_t)parachute.alt_min() * 100)) {
        return;
    }

    // check for angle error over 30 degrees
    const float angle_error = attitude_control->get_att_error_angle_deg();
    Log_Write_Data(102, angle_error);
    if (angle_error <= CRASH_CHECK_ANGLE_DEVIATION_DEG) {
        uint32_t now = AP_HAL::millis();

        if (control_loss_count > 0) {
            control_loss_count--;
            Log_Write_Data(100, control_loss_count);

            if (now - last_gcs_sent > 500 || last_gcs_type != 5) {
                gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: good angle error, reduce counter: %d", control_loss_count);
                last_gcs_sent = now;
                last_gcs_type = 5;
            }
        }

        if (now - last_gcs_sent > 500 || last_gcs_type != 5) {
            gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: good angle error, counter is 0");
            last_gcs_sent = now;
            last_gcs_type = 5;
        }
        return;
    }

    // increment counter
    if (control_loss_count < (uint32_t(g2.parachute_trigger_delay) * scheduler.get_loop_rate_hz() / 1000)) {
        control_loss_count++;
        Log_Write_Data(100, control_loss_count);

        uint32_t now = AP_HAL::millis();
        if (now - last_gcs_sent > 500 || last_gcs_type != 6) {
            gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: increase counter: %d", control_loss_count);
            last_gcs_sent = now;
            last_gcs_type = 6;
        }
    } else { // loss of control for at least 1 second
        // reset control loss counter
        control_loss_count = 0;
        Log_Write_Data(100, control_loss_count);

        uint32_t now = AP_HAL::millis();
        if (now - last_gcs_sent > 500 || last_gcs_type != 7) {
            gcs().send_text(MAV_SEVERITY_DEBUG, "PARACHUTE: loss of control! Releasing!");
            last_gcs_sent = now;
            last_gcs_type = 7;
        }
        // log an error in the dataflash
        Log_Write_Error(ERROR_SUBSYSTEM_CRASH_CHECK, ERROR_CODE_CRASH_CHECK_LOSS_OF_CONTROL);
        // release parachute
        parachute_release();
    }
}

// parachute_release - trigger the release of the parachute, disarm the motors and notify the user
void Copter::parachute_release()
{
    // send message to gcs and dataflash
    gcs().send_text(MAV_SEVERITY_INFO,"Parachute: Released");
    Log_Write_Event(DATA_PARACHUTE_RELEASED);

    // disarm motors
    init_disarm_motors();

    // release parachute
    parachute.release();

    // deploy landing gear
    landinggear.set_position(AP_LandingGear::LandingGear_Deploy);
}

// parachute_manual_release - trigger the release of the parachute, after performing some checks for pilot error
//   checks if the vehicle is landed 
void Copter::parachute_manual_release()
{
    // exit immediately if parachute is not enabled
    if (!parachute.enabled()) {
        return;
    }

    // do not release if vehicle is landed
    // do not release if we are landed or below the minimum altitude above home
    if (ap.land_complete) {
        // warn user of reason for failure
        gcs().send_text(MAV_SEVERITY_INFO,"Parachute: Landed");
        // log an error in the dataflash
        Log_Write_Error(ERROR_SUBSYSTEM_PARACHUTE, ERROR_CODE_PARACHUTE_LANDED);
        return;
    }

    // do not release if we are landed or below the minimum altitude above home
    if ((parachute.alt_min() != 0 && (current_loc.alt < (int32_t)parachute.alt_min() * 100))) {
        // warn user of reason for failure
        gcs().send_text(MAV_SEVERITY_ALERT,"Parachute: Too low");
        // log an error in the dataflash
        Log_Write_Error(ERROR_SUBSYSTEM_PARACHUTE, ERROR_CODE_PARACHUTE_TOO_LOW);
        return;
    }

    // if we get this far release parachute
    parachute_release();
}

#endif // PARACHUTE == ENABLED
