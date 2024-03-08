#include <linux/module.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/console.h>

#define MAX_ARG_STRINGS 0x7FFFFFFF

DEFINE_LED_TRIGGER(fork_blink);

unsigned long addr;
module_param(addr, ulong, 0);
MODULE_PARM_DESC(addr, "Address of the `sys_call_table` symbol");

typedef asmlinkage long (*sys_call_ptr_t)(const struct pt_regs *);
static sys_call_ptr_t *sys_call_table_ptr;
sys_call_ptr_t real_clone;
sys_call_ptr_t real_execve;

extern unsigned long __force_order;

static inline void force_write_cr0(unsigned long val)
{
	asm volatile("mov %0,%%cr0" : : "r"(val), "m"(__force_order));
}

static char *get_current_exe_file_string(char *buffer, size_t length)
{
	if (!buffer)
		return NULL;

	struct mm_struct *mm = current->mm;

	if (!mm)
		return NULL;

	char *p = NULL;

	down_read(&mm->mmap_lock);
	if (mm->exe_file) {
		p = d_path(&mm->exe_file->f_path, buffer, length);
	}
	up_read(&mm->mmap_lock);

	if (IS_ERR(p))
		return NULL;

	return p;
}

static void print_clone(void)
{
	char *pathname = kmalloc(PATH_MAX, GFP_KERNEL);
	if (pathname) {
		char *p = get_current_exe_file_string(pathname, PATH_MAX);
		if (p)
			pr_info("clone syscall initiated by %s\n", p);
		kfree(pathname);
	}
}

static void print_execve(const struct pt_regs *pr)
{
	char *buf = kmalloc(2 * PATH_MAX, GFP_KERNEL);

	if (!buf)
		return;

	char *original_exe_file = get_current_exe_file_string(buf, PATH_MAX);

	if (!original_exe_file)
		goto out;

	if (strncpy_from_user(buf + PATH_MAX, (char *)pr->di, PATH_MAX) < 0)
		goto out;

	console_lock();
	pr_info("%s ->[execve]-> %s\n", original_exe_file, buf + PATH_MAX);

	char __user **argv = (char **)pr->si;

	char __user *p = NULL;

	for (int i = 0; i < MAX_ARG_STRINGS; i++) {
		if (get_user(p, argv + i))
			break;

		if (!p)
			break;

		if (strncpy_from_user(buf, p, 2 * PATH_MAX) < 0)
			break;

		pr_info("argv[%d]: %s\n", i, buf);
	}
	console_unlock();
out:
	kfree(buf);
}

static long my_clone(const struct pt_regs *pr)
{
	static int state = 0;
	print_clone();
	led_trigger_event(fork_blink, (state ^= 1));
	return real_clone(pr);
}

static long my_execve(const struct pt_regs *pr)
{
	print_execve(pr);
	return real_execve(pr);
}

static int __init fork_blink_init(void)
{
	pr_debug("fork_blink module loading...\n");

	if (addr == 0)
		return -EINVAL;

	sys_call_table_ptr = (sys_call_ptr_t *)addr;

	real_clone = sys_call_table_ptr[__NR_clone];
	real_execve = sys_call_table_ptr[__NR_execve];

	// Temporarily disable write protection
	force_write_cr0(read_cr0() & (~0x10000));

	// Overwrite the syscall table entry
	sys_call_table_ptr[__NR_clone] = my_clone;
	sys_call_table_ptr[__NR_execve] = my_execve;

	// Re-enable write protection
	force_write_cr0(read_cr0() | 0x10000);

	led_trigger_register_simple("fork_blink", &fork_blink);

	pr_info("fork_blink module loaded.\n");
	return 0;
}

static void __exit fork_blink_exit(void)
{
	pr_debug("fork_blink module unloading....\n");

	led_trigger_unregister_simple(fork_blink);

	// Temporarily disable write protection
	force_write_cr0(read_cr0() & (~0x10000));

	// Overwrite the syscall table entry
	sys_call_table_ptr[__NR_clone] = real_clone;
	sys_call_table_ptr[__NR_execve] = real_execve;

	// Re-enable write protection
	force_write_cr0(read_cr0() | 0x10000);

	pr_info("fork_blink module unloaded.\n");
}

module_init(fork_blink_init);
module_exit(fork_blink_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thibaut Brintet");
MODULE_DESCRIPTION("Module overwriting the clone system call to blink leds "
		   "each time it is called.");
