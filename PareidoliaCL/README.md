# PareidoliaCL

**PareidoliaCL** is the user mode client for the **PareidoliaTriggerbot** driver.

## Usage

In the following sections, words with the *CFG_KEY_* prefix refer to keybinds defined in the [config file](../Common/config.h/).

### Loading

1. Load the **PareidoliaTriggerbot** driver.

2. Start the **PareidoliaCL** (client) executable.

3. Wait for the client to display the following lines:

    Waiting for Overwatch process. (Press CFG_KEY_EXIT_CLIENT_TEXT to exit client)

4. Start the Overwatch executable.

5. Wait for the client to detect the Overwatch process and display the following lines:

    Found Overwatch process. (ProcessId = OVERWATCH_PROCESS_ID)
    Press CFG_KEY_GET_OVERWATCH_CONTEXT_TEXT when Overwatch is at the main menu...

5. Wait for the Overwatch process to load to the main menu.

6. Press the CFG_KEY_GET_OVERWATCH_CONTEXT_TEXT keybind to capture the Overwatch context.

7. Wait for the client to display the following line:

    Starting TB.

8. The triggerbot remains in the tick loop until the user presses the CFG_KEY_EXIT_CLIENT keybind to exit the client.

Users must start the client before the Overwatch process so that the client can enable process access protection for itself to prevent Overwatch from reading its virtual address space.

### New Round Initialization

The triggerbot must be initialized for each new game round using the following steps:

1. Press the CFG_KEY_INITIALIZE_NEW_ROUND keybind to initiate the VivienneVMM capture execution context register (CECR) request.

2. Press the Widowmaker 'zoom' keybind and remain scoped until the VivienneVMM CECR request completes.

3. There are four possible states depending on the result of the VivienneVMM CECR request:

    i. **CECR FAILURE A**: The driver failed to service the request. Restart from step (1).

    ii. **CECR FAILURE B**: The request returned zero matches. This failure occurs when the user misses the CECR timing window by executing step (2) too early or too late. Users can adjust this timing window by increasing or decreasing the duration of the CECR request using the CFG_KEY_DECREASE_NEW_ROUND_SAMPLE_DURATION and CFG_KEY_INCREASE_NEW_ROUND_SAMPLE_DURATION keybinds. Restart from step (1).

    iii. **CECR FAILURE C**: The request returned multiple matches. This failure occurs when another user in the game has caused the trace state instruction to be executed during the CECR request. i.e., The CECR request has the trace state address for multiple players and cannot determine which trace state address corresponds to the local player. Wait a few seconds and restart from step (1).

    iv. **CECR SUCCESS**: The triggerbot was successfully initialized for the round. Press the CFG_KEY_TOGGLE_TRIGGERBOT to enable the triggerbot.

4. Verify that the triggerbot was initialized with the local player's trace state address using the following steps:

    i. Toggle the triggerbot to the active state.

    ii. Zoom, wait for 100% charge, and place the crosshair over a dynamic, non-player entity (e.g., a spawn door, the lid of a trashcan, the payload). The triggerbot should detect the entity under the crosshair and inject a left mouse click. If this does not occur then restart from step (1).

## Warnings

The client is intended to be used for one Overwatch process. i.e., The user should exit the client after the Overwatch process terminates. Behavior is undefined if the user attempts to use the client for multiple Overwatch processes.

### Desynchronization

The triggerbot may become desynchronized and exhibit strange behavior (e.g., injecting mouse clicks every second whether the local player is scoped or not). Users can fix this by following the steps to initialize a new round.

## Limitations

### Driver Certificate

The PareidoliaTriggerbot project does not contain a certificate issued by a trusted root authority. Users must enable test signing in order to load the PareidoliaTriggerbot driver.

## Notes

* The debug configuration uses the multi-threaded debug runtime library to reduce library requirements in virtual machines.
