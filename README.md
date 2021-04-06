# axi-timer-test
Use an AxiTimer IP (consult [xilinx documentation](https://www.xilinx.com/support/documentation/ip_documentation/axi_timer/v2_0/pg079-axi-timer.pdf)) to generate and exercise periodic, fabric-generated interrupts on Zynq.

The firmware is configured for the Digilent Zybo eval board and was generated with Vivado 2018.3.

1. Open vivado; chdir to this directory
2. In the TCL window type

       source axi_timer.tcl

    this should open the project.
3. You can open the block-design and view it; also shows the address map.
4. Synthesize, implement and generate programming file.
5. Upload to FPGA
6. Cross-compile `software/axi-timer.c`

In the `software/` subdirectory you find a program that exercises the timer (under linux).
It requires an appropriate device-tree (example in the `software/` directory for reference).
The program was tested under 4.14.139 RT_PREEMPT-66.

The program runs for a number of interrupts and records min/max interrupt-latency and -period.
