# 关于原lab4_challenge1中相对路径的bug

在原lab4_challenge1中，所有的parent路径查找时，都是使用的相对路径，即从当前目录开始查找文件。当前修改后正确的代码示例如下：

```C++
struct file *vfs_open(const char *path, int flags) {
    struct dentry *parent = get_path_start_dentry(path); // support relative path
    char miss_name[MAX_PATH_LEN];

    // path lookup.
    struct dentry *file_dentry = lookup_final_dentry(path, &parent, miss_name);
```

而原始lab4_challenge1代码如下：

```C++
struct file *vfs_open(const char *path, int flags) {
    struct dentry *parent = current->pfile->cwd;
    char miss_name[MAX_PATH_LEN];

    // path lookup.
    struct dentry *file_dentry = lookup_final_dentry(path, &parent, miss_name);
```

上述代码本身逻辑是没有问题的，但是在合并后的输出会出现卡死现象，输出如下所示：

```shell
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080011000, PKE kernel size: 0x0000000000011000 .
free physical memory address: [0x0000000080011000, 0x0000000087ffffff]
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080009000
kernel page table is on
RAMDISK0: base address of RAMDISK0 is: 0x0000000087f35000
RFS: format RAMDISK0 done!
Switch to user mode...
in alloc_proc. user frame 0x0000000087f29000, user stack 0x000000007ffff000, user kstack 0x0000000087f28000
FS: created a file management struct for a process.
in alloc_proc. build proc_file_management successfully.
User application is loading.
Application: /bin/app_relativepath
|
|---------> 卡死
```

这是因为在elf.c中的load_bincode_from_host_elf()函数中，调用了vfs_open()函数来打开输入的应用程序文件，而vfs_open()
函数中根据上面的修改，使用了相对路径current->pfile->
cwd来查找文件，而通过调试可以发现，此时current为空指针（直接读代码也会发现，因为还没将对应进程赋值给current），因此会导致访问空指针而卡死。

```C++
void load_bincode_from_host_elf(process *p, char *filename) {
    sprint("Application: %s\n", filename);

    // elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
    elf_ctx elfloader;
    // elf_info is defined above, used to tie the elf file and its corresponding process.
    elf_info info;

    info.f = vfs_open(filename, O_RDONLY);
    info.p = p;
    // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
    if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

    // init elfloader context. elf_init() is defined above.
    if (elf_init(&elfloader, &info) != EL_OK)
        panic("fail to init elfloader.\n");

    // load elf. elf_load() is defined above.
    if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

    // entry (virtual, also physical in lab1_x) address
    p->trapframe->epc = elfloader.ehdr.entry;

    // close the vfs file
    vfs_close(info.f);

    sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}
```

## Q1：那么lab4_challenge1中为什么没有出现这个问题呢？

那么问题来了，为什么在lab4_challenge1中没有出现这个问题呢？这是因为在lab4_challenge1中，输入的应用程序文件打开方式是调用的spike接口，使用spike_file_open()
函数来打开输入的应用程序文件，而spike_file_open()函数中是使用的另外一套更底层的文件IO方式，vfs_open()
函数是使用的虚拟文件系统建立在这些底层之上的统一接口，因此在lab4_challenge1中，由于加载应用程序和应用程序打开文件的接口不同，是不会有影响的。

## Q2：那么为什么不能直接使用vfs_root_dentry?

```C++
struct file *vfs_open(const char *path, int flags) {
    struct dentry *parent = vfs_root_dentry;
    char miss_name[MAX_PATH_LEN];

    // path lookup.
    struct dentry *file_dentry = lookup_final_dentry(path, &parent, miss_name);
```

那么上述示例代码为什么不行？这就很简单，这是lab4_challenge1要求解决的问题，即支持相对路径，如果直接使用vfs_root_dentry，那么就只能支持绝对路径了，无法支持相对路径了，文件寻找会从一开始就找错。

## Solution

所以可以使用一个非常凑巧的解决方式，直接根据路径预先进行判断，如果是绝对路径就使用vfs_root_dentry，如果是相对路径就使用current->
pfile->cwd，这样就可以同时支持绝对路径和相对路径了，代码如下：

```C++
static inline struct dentry *get_path_start_dentry(const char *path) {
    if (path[0] == '/') {
        // absolute path always starts from root
        return vfs_root_dentry;
    } else {
        // relative path: must have a valid current process
        if (current == NULL || current->pfiles == NULL) {
            panic("get_path_start_dentry: cannot use relative path without process context!\n");
        }
        return current->pfiles->cwd;
    }
}
```
