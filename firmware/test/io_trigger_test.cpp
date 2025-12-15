#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

// ============ ORIGINAL FUNCTION (for reference) ============
/*
void io_pin_trigger(
    uint8_t enable, uint8_t current_state,
    uint8_t on_hr, uint8_t on_min,
    uint8_t off_hr, uint8_t off_min,
    uint16_t current_hr, uint16_t current_min,
    uint16_t grace_min, bool on_boot,
    uint16_t address, const char *relay_str
) {
    if (!enable) {
        return;
    }

    const uint16_t MINUTES_IN_DAY = 24 * 60;
    uint16_t on_total_mins = on_hr * 60 + on_min;
    uint16_t off_total_mins = off_hr * 60 + off_min;
    uint16_t total_mins = current_hr * 60 + current_min;

    uint8_t desired_state = current_state;

    if (on_total_mins == off_total_mins) {
        return;
    
    } else if (!on_boot) {
        uint16_t minutes_since_on = 
            (total_mins - on_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;
        uint16_t minutes_since_off = 
            (total_mins - off_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;

        if (!current_state && (minutes_since_on <= grace_min)) {
            desired_state = 1;

        } else if (current_state && (minutes_since_off <= grace_min)) {
            desired_state = 0;
        }

    } else {
        if (on_total_mins < off_total_mins) {
            desired_state = ((total_mins >= on_total_mins) &&
                             (total_mins < off_total_mins));

        } else {
            desired_state = ((total_mins >= on_total_mins) ||
                             (total_mins < off_total_mins));
        }
    }

    if (current_state != desired_state) {
        printf("[TRIGGER] %s -> %s (boot=%d)\n", 
               relay_str, desired_state ? "ON" : "OFF", on_boot);
    }
}
*/

// ============ TEST LOGIC FUNCTION ============
bool compute_trigger_result(
    uint8_t enable, uint8_t current_state,
    uint8_t on_hr, uint8_t on_min,
    uint8_t off_hr, uint8_t off_min,
    uint16_t current_hr, uint16_t current_min,
    uint16_t grace_min, bool on_boot,
    bool* would_trigger, bool* new_state)
{
    printf("┌─ Schedule: %02d:%02d to %02d:%02d\n",
           on_hr, on_min, off_hr, off_min);
    printf("│  Current: %02d:%02d, State: %s\n", 
           current_hr, current_min, current_state ? "ON" : "OFF");
    printf("│  Grace: %d min, Boot: %s, Enable: %s\n",
           grace_min, on_boot ? "yes" : "no", enable ? "yes" : "no");
    
    if (!enable) {
        printf("│  Result: DISABLED (no action)\n");
        printf("└────────────────────────────────────────\n");
        *would_trigger = false;
        *new_state = current_state;
        return true;
    }

    const uint16_t MINUTES_IN_DAY = 24 * 60;
    uint16_t on_total_mins = on_hr * 60 + on_min;
    uint16_t off_total_mins = off_hr * 60 + off_min;
    uint16_t total_mins = current_hr * 60 + current_min;

    uint8_t desired_state = current_state;

    if (on_total_mins == off_total_mins) {
        printf("│  Logic: ON==OFF -> disabled schedule (no action)\n");
        printf("│  Result: DISABLED (schedule has zero duration)\n");
        printf("└────────────────────────────────────────\n");
        *would_trigger = false;
        *new_state = current_state;
        return true;
    }

    if (!on_boot) {
        uint16_t minutes_since_on = 
            (total_mins - on_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;
        uint16_t minutes_since_off = 
            (total_mins - off_total_mins + MINUTES_IN_DAY) % MINUTES_IN_DAY;

        if (!current_state && (minutes_since_on <= grace_min)) {
            desired_state = 1;
            printf("│  Logic: Within grace after ON (%d <= %d min)\n", 
                   minutes_since_on, grace_min);
            
        } else if (current_state && (minutes_since_off <= grace_min)) {
            desired_state = 0;
            printf("│  Logic: Within grace after OFF (%d <= %d min)\n", 
                   minutes_since_off, grace_min);

        } else {
            printf("│  Logic: No trigger needed, or outside the grace period\n");
        }

    } else {
        if (on_total_mins < off_total_mins) {
            bool should_be_on = (total_mins >= on_total_mins) && 
                                (total_mins < off_total_mins);
            desired_state = should_be_on;
            printf("│  Logic: Same-day schedule, should be %s\n",
                   should_be_on ? "ON" : "OFF");

        } else {
            bool should_be_on = (total_mins >= on_total_mins) || 
                                (total_mins < off_total_mins);
            desired_state = should_be_on;
            printf("│  Logic: Overnight schedule, should be %s\n",
                   should_be_on ? "ON" : "OFF");
        }
    }

    bool trigger = (current_state != desired_state);
    *would_trigger = trigger;
    *new_state = desired_state;
    
    if (trigger) {
        printf("│  Result: TRIGGER %s -> %s\n", 
               current_state ? "ON" : "OFF", desired_state ? "ON" : "OFF");

    } else {
        printf("│  Result: NO CHANGE (already %s)\n", 
               current_state ? "ON" : "OFF");
    }
    printf("└────────────────────────────────────────\n");
    
    return true;
}

// ============ TEST CASES ============
typedef struct {
    const char* name;
    uint8_t enable;
    uint8_t current_state;
    uint8_t on_hr, on_min;
    uint8_t off_hr, off_min;
    uint16_t current_hr, current_min;
    uint16_t grace_min;
    bool on_boot;
    bool expected_trigger;
    bool expected_new_state;
} TestCase;

// ============ RUN TEST CASE ============
bool run_test_case(TestCase* tc, int test_num) {
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║ TEST #%02d: %-46s ║\n", test_num, tc->name);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    
    bool would_trigger, new_state;
    compute_trigger_result(
        tc->enable, tc->current_state,
        tc->on_hr, tc->on_min, tc->off_hr, tc->off_min,
        tc->current_hr, tc->current_min,
        tc->grace_min, tc->on_boot,
        &would_trigger, &new_state
    );

    bool passed =
        (would_trigger == tc->expected_trigger) &&
        (new_state == tc->expected_new_state);

    printf("  Expected: %s -> %s\n",
           tc->expected_trigger ? "TRIGGER" : "NO TRIGGER",
           tc->expected_new_state ? "ON" : "OFF");
    printf("  Got:      %s -> %s\n",
           would_trigger ? "TRIGGER" : "NO TRIGGER",
           new_state ? "ON" : "OFF");
    printf("  Result:   %s\n", passed ? "PASS" : "FAIL");

    return passed;
}

// ============ MAIN TEST SUITE ============
int main() {
    TestCase test_suite[] = {
        // === BASIC FUNCTIONALITY ===
        {
            "Automation disabled",
            false, 0,  // enable=0, state=OFF
            9, 0, 18, 0,  // schedule 09:00-18:00
            10, 0, 5, false,  // time 10:00, grace=5, not boot
            false, false  // expected: no trigger, state unchanged
        },
        
        // === SAME-DAY SCHEDULE (NORMAL OPERATION) ===
        {
            "Same-day: ON at 09:02 (grace=5)",
            true, 0,  // OFF -> should turn ON
            9, 0, 18, 0,  // 09:00-18:00
            9, 2, 5, false,  // 09:02, grace=5
            true, true  // should trigger ON
        },
        {
            "Same-day: Already ON at 09:02",
            true, 1,  // Already ON
            9, 0, 18, 0,
            9, 2, 5, false,
            false, true  // no change
        },
        {
            "Same-day: OFF at 18:02 (grace=5)",
            true, 1,  // ON -> should turn OFF
            9, 0, 18, 0,
            18, 2, 5, false,
            true, false  // should trigger OFF
        },
        {
            "Same-day: Outside grace (09:10, grace=5)",
            true, 0,
            9, 0, 18, 0,
            9, 10, 5, false,
            false, false  // no trigger
        },
        
        // === SAME-DAY SCHEDULE (BOOT MODE) ===
        {
            "Boot: Same-day 10:00 (should be ON)",
            true, 0,
            9, 0, 18, 0,
            10, 0, 0, true,  // boot mode
            true, true  // should trigger ON
        },
        {
            "Boot: Same-day 08:00 (should be OFF)",
            true, 1,  // Currently ON, should be OFF
            9, 0, 18, 0,
            8, 0, 0, true,
            true, false  // should trigger OFF
        },
        {
            "Boot: Same-day exactly at 09:00",
            true, 0,
            9, 0, 18, 0,
            9, 0, 0, true,
            true, true  // should trigger ON
        },
        
        // === OVERNIGHT SCHEDULE (BOOT MODE) ===
        {
            "Boot: Overnight at 02:00 (should be ON)",
            true, 0,
            22, 0, 6, 0,  // 22:00-06:00
            2, 0, 0, true,
            true, true
        },
        {
            "Boot: Overnight at 07:00 (should be OFF)",
            true, 1,
            22, 0, 6, 0,
            7, 0, 0, true,
            true, false
        },
        {
            "Boot: Overnight at 06:00 (should be OFF)",
            true, 1,
            22, 0, 6, 0,
            6, 0, 0, true,
            true, false
        },
        {
            "Boot: Overnight at 22:00 (should be ON)",
            true, 0,
            22, 0, 6, 0,
            22, 0, 0, true,
            true, true
        },
        
        // === OVERNIGHT SCHEDULE (NORMAL OPERATION) ===
        {
            "Normal: Overnight grace after ON (23:02)",
            true, 0,
            23, 0, 6, 0,
            23, 2, 5, false,
            true, true
        },
        {
            "Normal: Overnight grace after OFF (06:02)",
            true, 1,
            23, 0, 6, 0,
            6, 2, 5, false,
            true, false
        },
        
        // === EDGE CASES ===
        {
            "Grace period crossing midnight",
            true, 0,
            23, 55, 6, 0,
            0, 2, 10, false,  // 00:02, 7 mins after 23:55
            true, true  // should trigger ON
        },
        {
            "Large grace period (120 min)",
            true, 0,
            9, 0, 18, 0,
            10, 30, 120, false,  // 1.5 hours after ON
            true, true
        },
        {
            "Zero grace period",
            true, 0,
            9, 0, 18, 0,
            9, 0, 0, false,
            true, true
        },
        {
            "Zero grace period, 1 min later",
            true, 0,
            9, 0, 18, 0,
            9, 1, 0, false,
            false, false  // no trigger
        },
        
        // === 24-HOUR SCHEDULE ===
        {
            "24-hour schedule (ON=OFF) at boot",
            true, 0,
            9, 0, 9, 0,  // same ON and OFF time
            10, 0, 5, true,
            false, false  // should stay OFF (schedule has zero duration)
        },
        {
            "24-hour schedule (ON=OFF) non-boot (OFF)",
            true, 0,
            9, 0, 9, 0,  // same ON and OFF time
            10, 0, 5, false,
            false, false  // disabled schedule -> no change
        },
        {
            "24-hour schedule (ON=OFF) non-boot (ON)",
            true, 1,
            9, 0, 9, 0,
            10, 0, 5, false,
            false, true  // disabled schedule -> remain ON (no trigger)
        },
        {
            "24-hour schedule (ON=OFF) non-boot with grace",
            true, 0,
            9, 0, 9, 0,
            9, 2, 5, false,
            false, false  // disabled schedule -> no trigger even within grace
        },
        
        // === MIDNIGHT BOUNDARY ===
        {
            "Midnight boundary (23:59 -> 00:01)",
            true, 1,
            23, 0, 1, 0,  // 23:00-01:00
            0, 1, 5, false,  // 00:01, 1 min after midnight
            false, true
        }
    };
    
    int total_tests = sizeof(test_suite) / sizeof(test_suite[0]);
    int passed_tests = 0;
    
    for (int i = 0; i < total_tests; i++) {
        bool passed = run_test_case(&test_suite[i], i + 1);
        if (passed) passed_tests++;
    }
    
    // ============ SUMMARY ============
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║%*sTEST SUMMARY%*s║\n", 46/2, "", (46+1)/ 2, "");
    printf("║══════════════════════════════════════════════════════════║\n");
    printf("║  Total tests   : %-39d ║\n", total_tests);
    printf("║  Passed        : %-39d ║\n", passed_tests);
    printf("║  Failed        : %-39d ║\n", total_tests - passed_tests);
    printf("║  Success rate  : %-39.f ║\n",
        (float)passed_tests / total_tests * 100);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    
    return (passed_tests == total_tests) ? 0 : 1; // Indicate failure
}
