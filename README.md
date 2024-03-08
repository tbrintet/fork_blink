# fork_blink
Linux Kernel Module intercepting clone and execve system calls.

System call arguments are logged to the kernel log buffer with printk,
which gives you the ability to monitor process execution on your machine.

To get a visual indication of the number of processes being launched, you
can configure a led to blink each time the clone system call is used.

# Compilation

In order to build kernel modules, you need to have a proper Linux kernel development environment.
You may need to install the linux-headers package, check your distro specific instructions.

```bash
make
```

# Usage
```bash
sudo insmod fork_blink.ko addr=0x$(sudo grep -w 'sys_call_table' /proc/kallsyms | cut -d' ' -f1)
```

**After module load**:

Choose the LED in the `/sys/class/leds/` directory. Check that it can actually
be controlled by `echo`'ing a few `0` or `1` values to its `brightness` file.
Finally, **in a root shell** (sudo does not work well with bash redirections),
assign the `fork_blink` LED trigger with:
```
echo fork_blink > /sys/class/leds/$YOUR_LED_HERE/trigger
```

Configuring your led is not mandatory if you are only interested in the logs.

Use the following command to read the kernel ring buffer and access the logs:
```
sudo dmesg
```

# Removal
To unload the module, use
```bash
sudo rmmod fork_blink
```
