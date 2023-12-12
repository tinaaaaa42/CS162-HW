1. Finding the faulting instruction
1.1 0xc0000008
1.2 0x80488ee
1.3 
080488e8 <_start>:
 80488e8:	55                   	push   %ebp
 80488e9:	89 e5                	mov    %esp,%ebp
 80488eb:	83 ec 18             	sub    $0x18,%esp
 80488ee:	8b 45 0c             	mov    0xc(%ebp),%eax   <= crash here
 80488f1:	89 44 24 04          	mov    %eax,0x4(%esp)
 80488f5:	8b 45 08             	mov    0x8(%ebp),%eax
 80488f8:	89 04 24             	mov    %eax,(%esp)
 80488fb:	e8 94 f7 ff ff       	call   8048094 <main>
 8048900:	89 04 24             	mov    %eax,(%esp)
 8048903:	e8 d3 21 00 00       	call   804aadb <exit>

1.4 It's in the start function in proj-pregame/src/lib/user/entry.c
```c
void _start(int argc, char* argv[]) { exit(main(argc, argv)); }
```
It's the entry of the program which calls the main function.

1.5 The instruction tries to copy the param of `_start` to `main`, and it
points to THE virtual address.


2. Step through the crash
2.1 main idle
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000edbc "\001", priority = 31, allelem = {prev = 0xc00
39d98 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0039d88 <fifo_ready_list>, next = 0xc0039d90 <fifo_ready_list+8>}, pcb = 0xc010500c, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f14 "", priority = 0, allelem = {prev = 0xc000e020
, next = 0xc0039da0 <all_list+8>}, elem = {prev = 0xc0039d88 <fifo_ready_list>, next = 0xc0039d90 <fifo_ready_list+8>}, pcb = 0x0, magic = 3446325067}

2.2
#0  0xc002d0a9 in process_execute (file_name=0xc0007d50 "do-nothing") at ../../userprog/process.c:57
#1  0xc0020a19 in run_task (argv=0xc0039c8c <argv+12>) at ../../threads/init.c:315
#2  0xc0020b57 in run_actions (argv=0xc0039c8c <argv+12>) at ../../threads/init.c:388
#3  0xc00203d9 in main () at ../../threads/init.c:136

315       process_wait(process_execute(task));

388         a->function(argv);

136       run_actions(argv);

2.3 do-nothing\000\000\000\000\000 0xc010b000
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_READY, name = "main", '\000' <repeats 11 times>, stack = 0xc000ec00 "", priority = 31, allelem = {prev = 0xc0039d98
<all_list>, next = 0xc0104020}, elem = {prev = 0xc0039d88 <fifo_ready_list>, next = 0xc0039d90 <fifo_ready_list+8>}, pcb = 0xc010500c, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f14 "", priority = 0, allelem = {prev = 0xc000e020
, next = 0xc010b020}, elem = {prev = 0xc0039d88 <fifo_ready_list>, next = 0xc0039d90 <fifo_ready_list+8>}, pcb = 0x0, magic = 3446325067}
pintos-debug: dumplist #2: 0xc010b000 {tid = 3, status = THREAD_RUNNING, name = "do-nothing\000\000\000\000\000", stack = 0xc010bfd4 "", priority = 31, allelem = {prev = 0xc0104020
, next = 0xc0039da0 <all_list+8>}, elem = {prev = 0xc0039d88 <fifo_ready_list>, next = 0xc000e028}, pcb = 0x0, magic = 3446325067}

2.4 
│   tid = thread_create(file_name, PRI_DEFAULT, start_process, fn_copy);

2.5
{edi = 0x0, esi = 0x0, ebp = 0x0, esp_dummy = 0x0, ebx = 0x0, edx = 0x0, ecx = 0x0, eax = 0x0, gs = 0x23, fs = 0x23, es = 0x23, ds = 0x23, vec_no = 0x0, error_code = 0x0, frame_pointer = 0x0, eip = 0x80488e8, cs = 0x1b, eflags = 0x202, esp = 0xc0000000, ss = 0x23}

2.6
The value of cs and ss need to be changed, so it has to be in the kernel mode.
Besides, iret works as returning from interrupt, which means it has to change from kernel mode to user mode.

2.7
eax            0x0      0
ecx            0x0      0
edx            0x0      0
ebx            0x0      0
esp            0xc0000000       0xc0000000
ebp            0x0      0x0
esi            0x0      0
edi            0x0      0
eip            0x80488e8        0x80488e8
eflags         0x202    [ IF ]
cs             0x1b     27
ss             0x23     35
ds             0x23     35
es             0x23     35
fs             0x23     35
gs             0x23     35

These are exactly the same with former `if_`

2.8
#0  _start (argc=-268370093, argv=0xf000ff53) at ../../lib/user/entry.c:6
#1  0xf000ff53 in ?? ()

3. Debug
3.3 0xbfffff98:     0x00000001      0x000000a2
3.4 args[0]=0x1 args[1]=0xa2