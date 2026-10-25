# prof

C/C++ code performance profiler for embedded systems based on GNU
instrumentation calls for stack tracing, Arm Cortex-M ITM (Instrumentation Trace
Macrocell) for streaming the data to host machine, a python script ``prof.py``
for parsing and folding the stack traces, and
[Speedscope](https://www.speedscope.app/) for visualization.

## Usage

1. Build and flash the firmware to the target MCU.

2. Generate symbols file from built firmware required for parsing the ITM trace
   data:

    ```bash
    arm-none-eabi-nm -lnC path/to/elf > path/to/elf.symbols
    ```

3. Collect and fold profile data. Iterative one-liner example: remove old data,
   reset the STM32F3 target MCU, wait for 1 second, record for 2 seconds and
   parse the data into a Speedscope JSON file. Some examples:

    ```
    set traceclk 72000000;set pinfreq 2000000;set duration_ms 2000;rm prof.dat;openocd -f modules/higgs/openocd.jlink.cfg -c "init
    ;reset;sleep 1000;stm32f3x.tpiu configure -protocol uart -output prof.dat -traceclk $traceclk -pin-freq $pinfreq;stm32f3x.tpiu enable;sleep $duration_ms;stm32f3x.tpiu disable;shutdown" && python3 extern/ln/ln/prof/tools/prof.py .build/higgs-rel/modules/higgs/higgs.symbols prof.dat --clk_freq $traceclk
    ```

    Or

    ```
    set traceclk 72000000; set pinfreq 2000000;
    rm prof.dat; openocd -f openocd.jlink.cfg -c "init;reset;sleep 1000;tpiu config internal prof.dat uart off 72000000 2000000;sleep 2000;shutdown" && python3 tools/prof.py path/to/elf.symbols prof.dat --clk_freq 72000000
    ```

    > 72000000 is the ITM baud rate in Hz, which is the same as the CPU clock
    > frequency. Set this to the correct value for your target MCU. 2000000 is
    > the ITM baud rate in Hz, which limit depends on the debug probe, the
    > target MCU and it's clock frequency. It should roughly be no more than
    > 1/10 of the CPU clock frequency and below the specified debug probe speed
    > capabilities.

4. The resulting file `prof.json` will contain the folded stack traces for each
   thread in the Speedscope format. You can open it in
   [Speedscope](https://www.speedscope.app/) to visualize the performance
   profile.

    > The `prof_0.json` will be the thread with the most stack calls, and the
    > `prof_1.json` will be the second most, and so on.

# TODO

- Draw ideas from Tracy underlying mechanism to improve throughput.
- Look for a better integration with Tracy utilizing it's possible features in
  useful ways.
- Research [Orbuculum](https://orbcode.org/orbuculum/swo-code-instrumentation)
  and the website in general - looks interesting.
- Consider creating a custom GUI (ImGUI for example). Speedscope and Tracy are
  useful tools, but a custom GUI could provide a more tailored and efficient
  visualization for specific use cases.
