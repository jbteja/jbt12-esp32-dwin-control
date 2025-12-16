#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

// ============ TEST LOGIC FUNCTION ============
bool compute_trigger_interval_result(
    uint8_t enable, uint8_t current_state,
    uint8_t on_hr, uint8_t on_min,
    uint8_t off_hr, uint8_t off_min,
    uint16_t current_hr, uint16_t current_min,
    uint16_t current_sec, uint16_t interval_hr,
    uint16_t duration_sec, uint32_t last_spray,
    uint32_t* next_last_spray, bool* would_trigger, bool* new_state
) {
    printf("┌─ Interval schedule: %02d:%02d to %02d:%02d\n", 
           on_hr, on_min, off_hr, off_min);
    printf("│  Current: %02d:%02d:%02d, State: %s\n", current_hr,
            current_min, current_sec, current_state ? "ON" : "OFF");
    printf("│  Interval: %d hrs, Duration: %d sec\n",
           interval_hr, duration_sec);
    printf("│  Enable: %s, Last spray: %s\n",
           enable ? "yes" : "no", last_spray == 0 ? "never" : "set");
    
    if (!enable) {
        printf("│  Result: DISABLED (no action)\n");
        printf("└────────────────────────────────────────\n");
        *would_trigger = false;
        *new_state = current_state;
        *next_last_spray = last_spray;  // No change
        return true;
    }

    if (duration_sec < 1 || duration_sec > 99) {
        printf("│  ERROR: Duration must be 1-99 seconds\n");
        printf("└────────────────────────────────────────\n");
        *would_trigger = false;
        *new_state = current_state;
        *next_last_spray = last_spray;
        return false;  // Return false to indicate error
    }

    if (interval_hr < 1 || interval_hr > 12) {
        printf("│  ERROR: Interval must be 1-12 hours\n");
        printf("└────────────────────────────────────────\n");
        *would_trigger = false;
        *new_state = current_state;
        *next_last_spray = last_spray;
        return false;
    }

    bool in_schedule = false;
    const uint16_t MINUTES_IN_DAY = 24 * 60;
    uint16_t on_total_mins = on_hr * 60 + on_min;
    uint16_t off_total_mins = off_hr * 60 + off_min;
    uint16_t total_mins = current_hr * 60 + current_min;

    if (on_total_mins == off_total_mins) {
        printf("│  Schedule: ON==OFF -> disabled schedule\n");
        in_schedule = false;

    } else if (on_total_mins < off_total_mins) {
        // Same-day schedule
        in_schedule = ((total_mins >= on_total_mins) && 
                       (total_mins < off_total_mins));
        printf("│  Schedule: %s (same-day window)\n",
               in_schedule ? "INSIDE" : "OUTSIDE");

    } else {
        // Overnight schedule  
        in_schedule = ((total_mins >= on_total_mins) || 
                       (total_mins < off_total_mins));
        printf("│  Schedule: %s (overnight window)\n",
               in_schedule ? "INSIDE" : "OUTSIDE");
    }

    uint8_t desired_state = current_state;
    uint32_t updated_last_spray = last_spray;
    bool schedule_triggered = false;

    if (!in_schedule) {
        // Outside spray schedule - should be OFF
        desired_state = 0;
        
        if (last_spray != 0) {
            printf("│  Logic: Outside schedule, resetting timer\n");
            updated_last_spray = 0;

        } else {
            printf("│  Logic: Outside schedule, timer already reset\n");
        }
        
    } else {
        // Inside schedule - interval logic applies
        uint32_t current_time_sec =
            (current_hr * 3600) + (current_min * 60) + current_sec;
        uint32_t interval_sec = interval_hr * 3600;
        
        if (last_spray == 0) {
            // First spray - should be ON
            printf("│  Logic: First spray in schedule\n");
            desired_state = 1;
            updated_last_spray = current_time_sec;
            schedule_triggered = true;
            
        } else {
            // Calculate time since last spray started
            uint32_t time_since_last_spray;
            
            if (current_time_sec >= last_spray) {
                time_since_last_spray = current_time_sec - last_spray;

            } else {
                // Handle midnight rollover
                time_since_last_spray = (86400 - last_spray) + current_time_sec;
            }
            
            printf("│  Time since last spray: %d sec\n", time_since_last_spray);

            if (current_state == 1) {
                // Currently spraying
                if (time_since_last_spray >= duration_sec) {
                    // Duration complete, should turn OFF
                    printf("│  Logic: Duration complete (%d >= %d sec)\n",
                           time_since_last_spray, duration_sec);
                    desired_state = 0;
                    updated_last_spray = current_time_sec;  // Reset for next interval
                    schedule_triggered = true;

                } else {
                    printf("│  Logic: Still spraying (%d < %d sec)\n",
                           time_since_last_spray, duration_sec);
                }
                
            } else {
                // Not currently spraying
                if (time_since_last_spray >= interval_sec) {
                    // Interval elapsed, should turn ON
                    printf("│  Logic: Interval elapsed (%d >= %d sec)\n",
                           time_since_last_spray, interval_sec);
                    desired_state = 1;
                    updated_last_spray = current_time_sec;  // Reset spray start time
                    schedule_triggered = true;

                } else {
                    printf("│  Logic: Waiting for interval (%d < %d sec)\n",
                           time_since_last_spray, interval_sec);
                }
            }
        }
    }

    // Determine if state change is needed
    bool trigger = (current_state != desired_state);
    *would_trigger = trigger;
    *new_state = desired_state;
    *next_last_spray = updated_last_spray;
    
    if (trigger) {
        printf("│  Result: TRIGGER %s -> %s",
               current_state ? "ON" : "OFF", desired_state ? "ON" : "OFF");
        printf(schedule_triggered ? " (interval logic)\n" : "\n");

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
    uint16_t current_hr;
    uint16_t current_min;
    uint16_t current_sec;
    uint16_t interval_hr; 
    uint16_t duration_sec;
    uint32_t last_spray;
    bool expected_trigger;
    bool expected_new_state;
} TestCase;

// ============ RUN TEST CASE ============
bool run_test_case(TestCase* tc, int test_num) {
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║ TEST #%02d: %-46s ║\n", test_num, tc->name);
    printf("╚══════════════════════════════════════════════════════════╝\n");
    
    bool would_trigger = false;
    bool new_state = tc->current_state;
    uint32_t next_last_spray = tc->last_spray;

    compute_trigger_interval_result(
        tc->enable, tc->current_state,
        tc->on_hr, tc->on_min,
        tc->off_hr, tc->off_min,
        tc->current_hr, tc->current_min,
        tc->current_sec, tc->interval_hr,
        tc->duration_sec, tc->last_spray,
        &next_last_spray, &would_trigger, &new_state
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
            0, 0,  // enable=0, state=OFF
            9, 0, 18, 0,  // schedule 09:00-18:00
            10, 0, 0,  // current time 10:00:00
            2, 30,  // interval=2hrs, duration=30s
            0,  // last_spray = never
            false, false  // expected: no trigger, state unchanged
        },
    
        // === SCHEDULE BOUNDARY CASES ===
        {
            "Outside schedule (before on)",
            1, 0,  // enabled, OFF
            10, 0, 18, 0,  // 10:00-18:00
            9, 0, 0, // current 09:00 (before schedule)
            2, 30,  // interval 2hrs, duration 30s
            36000,  // last_spray = 10:00:00 (36000 sec)
            false, false  // should turn OFF, reset timer
        },
        {
            "Outside schedule (after off)",
            1, 1,  // enabled, ON (should turn OFF)
            10, 0, 18, 0,  // 10:00-18:00
            19, 0, 0,  // current 19:00 (after schedule)
            2, 30,
            36000,
            true, false  // should trigger OFF
        },
        {
            "On time boundary (exact start)",
            1, 0,  // enabled, OFF
            10, 0, 18, 0,
            10, 0, 0,  // current exactly at start
            2, 30,
            0,  // never sprayed
            true, true  // should trigger ON (first spray)
        },
        {
            "Off time boundary (exact end)",
            1, 1,  // enabled, ON
            10, 0, 18, 0,
            18, 0, 0,  // current exactly at end
            2, 30,
            36000,  // last_spray at 10:00
            true, false  // should trigger OFF (outside schedule)
        },
    
        // === OVERNIGHT SCHEDULE ===
        {
            "Overnight schedule - inside (late)",
            1, 0,  // enabled, OFF
            22, 0, 6, 0,  // 22:00-06:00 overnight
            23, 0, 0,  // current 23:00
            2, 30,
            0,
            true, true  // should trigger ON
        },
        {
            "Overnight schedule - inside (early)",
            1, 0,
            22, 0, 6, 0,
            5, 0, 0,  // current 05:00 (still in schedule)
            2, 30,
            0,
            true, true  // should trigger ON
        },
        {
            "Overnight schedule - outside (mid-day)",
            1, 1,
            22, 0, 6, 0,
            14, 0, 0,  // current 14:00 (outside)
            2, 30,
            82800,  // last_spray at 23:00
            true, false  // should trigger OFF
        },
        
        // === INTERVAL LOGIC ===
        {
            "First spray in schedule",
            1, 0,
            9, 0, 18, 0,
            10, 0, 0,  // 10:00
            2, 30,  // interval 2hrs
            0,  // never sprayed
            true, true  // should trigger ON
        },
        {
            "Still spraying (duration not complete)",
            1, 1,  // currently ON
            9, 0, 18, 0,
            10, 0, 15, // 10:00:15 (15 seconds after start)
            2, 30,
            36000,  // started at 10:00:00
            false, true  // no change, still ON
        },
        {
            "Duration complete (should turn OFF)",
            1, 1,  // currently ON
            9, 0, 18, 0,
            10, 0, 45, // 10:00:45 (45 seconds after start)
            2, 30,
            36000,  // started at 10:00:00
            true, false  // trigger OFF
        },
        {
            "Waiting for interval (not elapsed)",
            1, 0,  // currently OFF
            9, 0, 18, 0,
            11, 30, 0,  // 11:30 (1.5hrs after 10:00)
            2, 30,  // interval 2hrs
            36000,  // last_spray at 10:00:00
            false, false  // no change, still OFF
        },
        {
            "Interval elapsed (should turn ON)",
            1, 0,  // currently OFF
            9, 0, 18, 0,
            12, 15, 0,  // 12:15 (2.25hrs after 10:00)
            2, 30,  // interval 2hrs
            36000,  // last_spray at 10:00:00
            true, true  // trigger ON
        },
        
        // === EDGE CASES ===
        {
            "Zero interval (should spray continuously)",
            1, 0,
            9, 0, 18, 0,
            10, 0, 0,
            0, 30,  // interval 0hrs = immediate
            0,
            false, false // no trigger, stays OFF
        },
        {
            "Zero duration (instant spray)",
            1, 0,
            9, 0, 18, 0,
            10, 0, 0,
            2, 0,  // duration 0sec
            0,
            false, false  // no trigger, stays OFF
        },
        {
            "Schedule with same on/off time",
            1, 0,
            10, 0, 10, 0,  // same time = disabled
            10, 0, 0,
            2, 30,
            36000,
            false, false  // schedule disabled
        },
        
        // === MIDNIGHT ROLLOVER ===
        {
            "Midnight rollover - time since spray calc",
            1, 0,  // OFF
            22, 0, 6, 0,  // overnight
            1, 0, 0,  // 01:00
            1, 30,  // interval 1hr
            82800,  // last_spray at 23:00 (82800 sec)
            true, true  // FIXED: should trigger ON (2hrs > 1hr interval)
        },
        
        // Fixed midnight test:
        {
            "Midnight rollover - trigger ON",
            1, 0,
            22, 0, 6, 0,
            1, 0, 0,  // 01:00
            1, 30,  // interval 1hr
            82800,  // last_spray at 23:00 (2hrs ago)
            true, true  // should trigger ON (2hrs > 1hr interval)
        },
        
        // === COMPLEX SCENARIOS ===
        {
            "Cycle: ON -> wait -> OFF -> wait -> ON",
            1, 1,  // starting ON
            9, 0, 18, 0,
            9, 0, 30,  // 09:00:30
            1, 15,  // interval 1hr, 15s duration
            32400,  // last_spray at 09:00:00
            true, false  // duration complete, trigger OFF
        },
        {
            "Timer reset when leaving schedule",
            1, 0,
            10, 0, 12, 0,
            13, 0, 0,  // 13:00 (outside)
            1, 30,
            36000,  // last_spray at 10:00
            false, false  // no trigger, but timer reset
        },

        // === DURATION BOUNDARY TESTS ===
        {
            "Duration boundary test - minimum duration",
            1, 1,
            9, 0, 18, 0,
            10, 0, 1,  // 10:00:01
            2, 1,  // duration = 1 sec
            36000,  // last_spray at 10:00:00
            true, false  // should trigger OFF
        },
        {
            "Duration boundary test - maximum duration",
            1, 1,
            9, 0, 18, 0,
            10, 1, 40,  // 10:01:40
            2, 99,  // duration = 99 sec
            36000,  // last_spray at 10:00:00
            true, false  // should trigger OFF
        },

        // === INTERVAL RANGE TESTS (1-12 hours) ===
        {
            "Minimum interval (1 hour)",
            1, 0,  // OFF
            9, 0, 18, 0,
            10, 0, 0,  // 10:00
            1, 30,  // 1 hour interval
            32400,  // last_spray at 09:00:00
            true, true  // should trigger ON (1hr elapsed)
        },
        {
            "Maximum interval (12 hours)",
            1, 0,  // OFF
            9, 0, 21, 1,  // longer schedule
            21, 0, 0,  // 21:00 (12hrs after 09:00)
            12, 30,  // 12 hour interval
            32400,  // last_spray at 09:00:00
            true, true  // should trigger ON
        },

        // === PRECISE TIMING TESTS (with seconds) ===
        {
            "Exact duration match with seconds",
            1, 1,  // ON
            9, 0, 18, 0,
            10, 0, 29,  // 10:00:29
            2, 30,
            36000,  // started at 10:00:00
            false, true  // NOT trigger (29 < 30)
        },
        {
            "Exact duration complete with seconds",
            1, 1,  // ON
            9, 0, 18, 0,
            10, 0, 30,  // 10:00:30 exactly
            2, 30,
            36000,  // started at 10:00:00
            true, false  // trigger OFF (30 >= 30)
        },
        {
            "Just before interval with seconds",
            1, 0,  // OFF
            9, 0, 18, 0,
            11, 59, 59,  // 11:59:59
            2, 30,
            36000,  // last_spray at 10:00:00
            false, false  // NOT trigger (1:59:59 < 2:00:00)
        },
        {
            "Exact interval with seconds",
            1, 0,  // OFF
            9, 0, 18, 0,
            12, 0, 0,  // 12:00:00 exactly
            2, 30,
            36000,  // last_spray at 10:00:00
            true, true  // trigger ON (2hrs elapsed)
        },

        // === SCHEDULE EDGE CASES ===
        {
            "Schedule ends at midnight (00:00)",
            1, 1,  // ON
            22, 0, 0, 0,  // 22:00 to 00:00
            23, 30, 0,
            2, 30,
            79200,  // started at 22:00:00
            true, false  // should trigger OFF
        },
        {
            "Schedule crosses midnight boundary",
            1, 0,  // OFF
            23, 30, 1, 30,  // 23:30 to 01:30
            0, 45, 0,  // 00:45 (inside overnight)
            1, 30,
            0,
            true, true  // first spray
        },

        // === STATE PERSISTENCE TESTS ===
        {
            "Already ON at schedule start",
            1, 1,  // already ON
            10, 0, 18, 0,
            10, 0, 0,
            2, 30,
            0,  // never sprayed (but already ON!)
            false, true  // no trigger, stay ON
        },
        {
            "Already OFF at schedule end",
            1, 0,  // already OFF
            10, 0, 18, 0,
            18, 0, 0,
            2, 30,
            36000,
            false, false  // no trigger, already OFF
        }
    };

    int total_tests = sizeof(test_suite) / sizeof(test_suite[0]);
    int passed_tests = 0;

    for (int i = 0; i < total_tests; i++) {
        bool passed = run_test_case(&test_suite[i], i + 1);
        if (passed) {
            passed_tests++;
        }
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
