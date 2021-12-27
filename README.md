# PCIe-PowerOn-Delay
Automated power-on delay for an 1:4 PCIe bus expander using the POST-LEDs of a Dell T3500

The script was written to enable using more than 7 GPUs with a Dell Precision T3500 Desktop.
Due to an issue with the BIOS, the system will not boot when more than 7 GPUs are connected, instead it will loop in the boot sequence forever (visible via repeating patterns of the POST debug LEDs).
However if the system powers up with 7 (or less) GPUs and has passed a certain point in the BIOS boot process, additional GPUs may be connected to the bus and the system runs fine afterwards.
The OS is required to re-enumerate the bus during boot to recognize the additional GPUs.

<br>
The power-on delay is realized using an Arduino Uno which "reads" the POST LEDs and controls the Power Enable line of the bus expander.
