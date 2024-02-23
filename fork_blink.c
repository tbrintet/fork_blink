#include <linux/module.h>
#include <linux/leds.h>

DEFINE_LED_TRIGGER(fork_blink);

unsigned long addr;
module_param(addr, ulong, 0);
MODULE_PARM_DESC(addr, "Address of the `sys_call_table` symbol");

typedef asmlinkage long (*sys_call_ptr_t)(const struct pt_regs *);
static sys_call_ptr_t *sys_call_table;
sys_call_ptr_t real_clone;

extern unsigned long __force_order;

static inline void force_write_cr0(unsigned long val)
{
	asm volatile("mov %0,%%cr0" : : "r"(val), "m"(__force_order));
}

static long my_clone(const struct pt_regs *pr)
{
	static int state = 0;
	pr_info("my_clone called\n");
	led_trigger_event(fork_blink, (state ^= 1));
	return real_clone(pr);
}

static int __init fork_blink_init(void)
{
	pr_info("fork_blink module loading...\n");

	if (addr == 0)
		return -EINVAL;

	sys_call_table = (sys_call_ptr_t *)addr;

	real_clone = sys_call_table[__NR_clone];

	// Temporarily disable write protection
	force_write_cr0(read_cr0() & (~0x10000));

	// Overwrite the syscall table entry
	sys_call_table[__NR_clone] = my_clone;

	// Re-enable write protection
	force_write_cr0(read_cr0() | 0x10000);

	led_trigger_register_simple("fork_blink", &fork_blink);
	led_trigger_event(fork_blink, LED_FULL);
	pr_info("fork_blink module loaded.\n");
	return 0;
}

static void __exit fork_blink_exit(void)
{
	pr_info("fork_blink module unloading....\n");

	led_trigger_unregister_simple(fork_blink);

	// Temporarily disable write protection
	force_write_cr0(read_cr0() & (~0x10000));

	// Overwrite the syscall table entry
	sys_call_table[__NR_clone] = real_clone;

	// Re-enable write protection
	force_write_cr0(read_cr0() | 0x10000);

	pr_info("fork_blink module unloaded.\n");
}

module_init(fork_blink_init);
module_exit(fork_blink_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thibaut Brintet");
MODULE_DESCRIPTION(
	"Module overwriting the clone system call to blink leds each time it is called.");
