# PareidoliaTriggerbot

PareidoliaTriggerbot is an external Widowmaker triggerbot which uses the [VivienneVMM](https://github.com/changeofpace/VivienneVMM) and [MouClassInputInjection](https://github.com/changeofpace/MouClassInputInjection) projects to bypass the Overwatch Anti-Cheat.

This hack has remained undetected since its initial completion in early 2018.

[![PareidoliaTriggerbot Clip](https://img.youtube.com/vi/XqaSmmckqps/maxresdefault.jpg)](https://www.youtube.com/watch?v=XqaSmmckqps "PareidoliaTriggerbot Clip")

## Usage

See the [PareidoliaCL README](./PareidoliaCL/README.md).

## Implementation

PareidoliaTriggerbot is composed of a user mode client application and a kernel mode driver. The client contains the triggerbot logic, and the driver provides support services for the client (e.g., reading process memory without a handle) via a device interface.

### Triggerbot Logic

The client executes an infinite tick loop to update the triggerbot state machine. The following diagram depicts a simplified overview of the tick loop:

                    .-----------------------.
                    |                       |
                    v                       |
    +===============================+       |
    |      Process user input       |       |
    +===============================+       |
                    |                       |
                    v                       |
    .-------------------------------.       |
    |  Is the round context valid?  |--No-->+
    '-------------------------------'       |
                    |                       |
                   Yes                      |
                    |                       |
                    v                       |
    .-------------------------------.       |
    |  Is the triggerbot enabled?   |--No-->+
    '-------------------------------'       |
                    |                       |
                   Yes                      |
                    |                       |
                    v                       |
    +===============================+       |
    |  Read Widowmaker Trace State  |       |
    +===============================+       |
                    |                       |
                    v                       |
    .-------------------------------.       |
    |   Is the player's crosshair   |--No-->+
    |     over an enemy player?     |       |
    '-------------------------------'       |
                    |                       |
                   Yes                      |
                    |                       |
                    v                       |
    +===============================+       |
    |       Activate trigger        |       |
    +===============================+       |
                    |                       |
                    v                       |
    +===============================+       |
    |       Trigger cooldown        |       |
    +===============================+       |
                    |                       |
                    '-----------------------'

### Widowmaker Trace State

The Widowmaker trace state is a ULONG-sized variable which has a nonzero value when:

1. There is a Widowmaker player in a match.

2. The Widowmaker player has the Widow's Kiss sniper rifle equipped.

3. The Widowmaker player is scoped (i.e., zoomed in) and the Widow's Kiss is fully charged.

4. The Widowmaker player's crosshair is over an enemy player entity or a dynamic, non-player entity (e.g., the payload, a closed spawn door, or the lid of a trash can).

This variable exists as a field in the trace state object type. The following diagram depicts the trace state elements inside the Overwatch virtual address space:


          Low Memory
    +====================+
    |                    |       Trace State Object
    |                    |               ~
    |                    |        +=============+
    |                    |        |             |         Trace State
    |--------------------|        |-------------|              ~
    | Dynamic Allocation | -----> | Trace State | -----> [0, MAXULONG]
    |--------------------|        |   Variable  |
    |                    |        |-------------|
    |                    |        |             |
    |                    |        +=============+
    |                    |
    |                    |
    |                    |
    |                    |
    |                    |
    +====================+
          High Memory


The game engine maintains one trace state object for each Widowmaker player in a match. A trace state object is created each time a non-Widowmaker player swaps to the Widowmaker hero. The object is destroyed when the player swaps to a non-Widowmaker hero, the round ends, or the player leaves the match.

NOTE We do not fully understand the purpose of the trace state variable. We refer to this concept as the 'trace state' because the variable behaves as if it were the hit detection result of the game engine running a trace ray whose origin vector is the Widowmaker player's crosshair.

### Widowmaker Trace State Instruction

The trace state mechanic provides all of the functionality required to write a triggerbot. In order to use this mechanic we need to be able to locate the address of our (local player) trace state object inside the remote Overwatch process. This is nontrivial for the following reasons:

1. The address of the target trace state object changes when the object is destroyed and recreated.

2. We cannot hook code in the Overwatch process because the anti-cheat frequently scans for code patches.

3. Overwatch's code and data are significantly obfuscated.

We can reliably recover the address of our trace state object using the VivienneVMM [Capture Execution Context Register](https://github.com/changeofpace/VivienneVMM#capture-execution-context) request. Our target instruction is executed whenever a Widowmaker player presses their 'zoom' keybind. The following is the annotated assembly of the target instruction:

    Platform:   Windows 7 x64
    File Name:  Overwatch.exe
    Version:    1.41.1.0 - 63372
    SHA256:

        9d079af7df9ed332f32d303c1eec0aab886f300dc79489019b8bbae682cbdb99

    Assembly:

        89 87 FC 01 00 00       mov     [rdi+1FCh], eax

    rdi = Pointer to the base address of a trace state object.
    1FC = The field offset of the trace state variable.
    eax = The new trace state value.

NOTE We found this instruction by scanning Overwatch's virtual memory for values which changed when the local player was scoped / not scoped.

NOTE We do not fully understand the purpose of the trace state instruction or its containing function.

### Anti-Cheat Bypass

PareidoliaTriggerbot passively avoids detection using the following strategies:

1. The client interacts with the target Overwatch process using the driver interface. i.e., The client does not open any handles to the target Overwatch process.

2. The client uses the MouClassInputInjection project for stealthy mouse input injection.

3. The driver registers kernel object callbacks to prevent the target Overwatch process from reading the client's virtual address space.

4. The triggerbot simulates realistic mouse clicks by waiting for a dynamic amount of time before injecting the mouse release event. This release delay is a pseudo-random number bounded by parameters in the [config file](./Common/config.h). Users can use the [MouHidInputHook](https://github.com/changeofpace/MouHidInputHook) project to obtain the average delay for their mouse.

## Projects

This repository uses the [MouClassInputInjection](https://github.com/changeofpace/MouClassInputInjection) and [VivienneVMM](https://github.com/changeofpace/VivienneVMM) projects as git submodules. The client and driver projects in these submodules are configured to produce static libraries. The PareidoliaTriggerbot projects link against these libraries for mouse input injection and capture execution context register requests.

## PareidoliaCL

The user mode triggerbot client.

## PareidoliaTriggerbot

The kernel mode triggerbot driver.

## MouClassInputInjection

A kernel mode static library which implements mouse input injection. This project is a submodule.

## MouiiCL

A user mode static library which provides the client interface for mouse input injection. This project is a submodule.

## VivienneCL

A user mode static library which provides the VivienneVMM interface for Capture Execution Context Register requests. This project is a submodule.

## VivienneVMM

A kernel mode static library which implements the VivienneVMM interface. This project is a submodule.

## Warnings

### Names

The projects in this repository use several globally visible names which can potentially be used as a detection vector by anti-cheat software. These names include:

* The symbolic link for the PareidoliaTriggerbot device object.
* The symbolic link for the MouClassInputInjection device object.
* The symbolic link for the VivienneVMM device object.
* The VivienneVMM signature returned by the cpuid instruction.
* The VivienneVMM log file name.

Users should modify these names and values by editing the [config file](./Common/config.h).

### Signature Scanning

The **PareidoliaCL.exe** and **PareidoliaTriggerbot.sys** files on disk are subject to signature scans (even with the process access protection module enabled). Users should pack and/or encrypt the **PareidoliaCL.exe** file and delete the **PareidoliaTriggerbot.sys** file after the driver is loaded to avoid this detection vector.

### Mouse Input Injection

The **PareidoliaTriggerbot** driver does not prevent successive **MOUSE_LEFT_BUTTON_DOWN** or **MOUSE_LEFT_BUTTON_UP** mouse input packets. Normally, a physical mouse will never generate two **MOUSE_LEFT_BUTTON_DOWN** packets or two **MOUSE_LEFT_BUTTON_UP** packets in a row because each physical click action has a press and release event. Since the PareidoliaTriggerbot driver injects mouse input packets it is possible for the following series of events to occur:

1. **PareidoliaCL** decides to trigger.
2. **PareidoliaCL** sends an IO request to the driver to inject a left-click input packet.
3. **PareidoliaTriggerbot** processes the IO request and injects a **MOUSE_LEFT_BUTTON_DOWN** packet.
4. The user physically presses the left mouse button.

In this scenario, steps (3) and (4) result in two successive **MOUSE_LEFT_BUTTON_DOWN** packets in the input data stream. Anti-cheat software can potentially use this unnatural series of events as a detection vector by monitoring mouse input events via Windows input hooks.

Users can mitigate this scenario by not clicking while the local player is scoped and the triggerbot is active.

## Limitations

### Process Access Manager

The Process Access Protection module may cause undesirable side effects. e.g., The user will be unable to close the command prompt process that is hosting the client by clicking the red 'X' button. This feature can be disabled by modifying the config file.

### Driver Certificate

The PareidoliaTriggerbot project does not contain a certificate issued by a trusted root authority. Users must enable test signing in order to load the PareidoliaTriggerbot driver.

## Disclaimer

This project is intended for educational purposes. Use at your own risk.

## Notes

* All projects in this repository were developed and tested on Windows 7 x64 SP1.
* All binaries are PatchGuard safe on Windows 7.
