#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YIGITHAN-FURKAN");
MODULE_DESCRIPTION("A module to traverse the process tree");

int pid;
module_param(pid, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(pid, "PID of the process to start the traversal");

static void traverse_process_tree(struct task_struct *task, int depth) {
    struct task_struct *child;
    struct list_head *list;

    printk(KERN_INFO "%*s- [%d] %s, start time: %llu\n", depth * 2, "", task->pid, task->comm, (unsigned long long)task->start_time);

    list_for_each(list, &task->children) {
        child = list_entry(list, struct task_struct, sibling);
        traverse_process_tree(child, depth + 1);
    }
}

int process_tree_init(void) {
    struct task_struct *task;

    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (task) {
        printk(KERN_INFO "Process tree for PID %d:\n", pid);
        traverse_process_tree(task, 0);
    } else {
        printk(KERN_ERR "Invalid PID: %d\n", pid);
    }

    return 0;
}

void process_tree_exit(void) {
    printk(KERN_INFO "Process tree module unloaded.\n");
}

module_init(process_tree_init);
module_exit(process_tree_exit);
