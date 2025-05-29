#ifndef config_h
#define config_h

// Pin Definitions
#define STEPPER_PULSE     33
#define STEPPER_DIR       34
#define STEPPER_ENABLE    -1
#define STEPPER_HOME      36

//Mechanism
#define PHYSICAL_TRAVEL   360
#define KEEPOUT_DISTANCE  5.0

// Calculation Aid:
#define STEP_PER_REV      800      // How many steps per revolution of the motor (S1 off, S2 on, S3 on, S4 off)
#define PULLEY_TEETH      30       // How many teeth has the pulley
#define BELT_PITCH        3        // What is the timing belt pitch in mm
#define RPM               1500
#define MAX_RPM           RPM*(1)  // Maximum RPM of motor
#define STEP_PER_MM       (STEP_PER_REV) / (PULLEY_TEETH * BELT_PITCH)
#define STEP_PER_MM       8.978675 // Overide with Measured STP/MM 
#define MAX_SPEED         (MAX_RPM / 60.0) * PULLEY_TEETH * BELT_PITCH
#define MAX_ACCEL         10000

#endif
