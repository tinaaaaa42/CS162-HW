#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static bool is_valid_user_pointer(const void* pointer);
static void check_args(uint32_t* arg);

static void syscall_handler(struct intr_frame*);

static void exit_handler(uint32_t*);
static void write_handler(uint32_t*, uint32_t*);

static bool is_valid_user_pointer(const void* pointer) {
  if (pointer == NULL) {
    return false;
  }
  if (!is_user_vaddr(pointer + sizeof(uint32_t) - 1)) {
    return false;
  }
  if (!pagedir_get_page(thread_current()->pcb->pagedir, pointer + sizeof(uint32_t) - 1)) {
    return false;
  }
  return true;
}

static void check_args(uint32_t* arg) {
  if (!is_valid_user_pointer(arg)) {
    exit_handler((uint32_t[]) {SYS_EXIT, -1});
  }
}
  
void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

static void syscall_handler(struct intr_frame* f UNUSED) {
  uint32_t* args = ((uint32_t*)f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  if (!is_valid_user_pointer(args)) {
    exit_handler((uint32_t[]) {SYS_EXIT, -1});
  }

  if (args[0] == SYS_EXIT) {
    check_args(&args[1]);
    exit_handler(args);
  } else if (args[0] == SYS_WRITE) {
    write_handler(args, &f->eax);
  }
}

static void exit_handler(uint32_t* args) {
  int status = args[1];
  printf("%s: exit(%d)\n", thread_current()->pcb->process_name, status);
  process_exit();
  NOT_REACHED();
}

static void write_handler(uint32_t* args, uint32_t* eax) {
  int fd = args[1];
  const void* buffer = (const void*) args[2];
  unsigned size = args[3];

  if (fd == 1) {
    putbuf(buffer, size);
    *eax = size;
  }
}