# fork_blink
Linux Kernel Module blinking leds each time the fork system call is used.

# Compilation
```bash
make
```

# Usage
```bash
sudo insmod fork_blink.ko addr=0x$(echo $(sudo grep 'sys_call_table' /proc/kallsyms | cut -d' ' -f1) | cut -d' ' -f1)
```

After module load:

Choose the LED in the `/sys/class/leds/` directory. Check that it can actually
be controlled by `echo`'ing a few `0` or `1` values to its `brightness` file.
Finally, assign the `fork_blink` LED trigger with:
```
# echo fork_blink > /sys/class/leds/$YOUR_LED_HERE/trigger
```

To remove the module, use
```bash
sudo rmmod fork_blink
```
