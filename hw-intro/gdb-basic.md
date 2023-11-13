1. Run `gdb map`
2. Execute `b main`
3. Execute `r`
4. First I use `disas` to look at the assembly code, and the two lines before are:
```asm
mov %edi, -0x24(%rbp)
mov %rsi, -0x30(%rbp)
```
Then execute `print $edi` and find out $edi = 0x1, which is clearly `argc`. So, executing `print $rsi`, we could find out `argv` stores `0x7fffffffe3e8`.

Another easier way is executing `info stack`, which prints out:
```
#0 main (argc=1. argv=0x7fffffffe3e8) at map.c:16
```

5. As the type of `argv` is `char *argv[]`, we should use `x/2wx $rsi` to find the address  and execute `print (char *) 0x7fffffffe647`. And it's the path of our running program!

6. Execute `b recur` and `continue`
7. Use `disas` to find the address 0x00005555555546cd
8. 9. `next`
10. `disas`
11. several `nexti`
12. Use `info r` to check out, and the most important register would undoubtedly be the param $rdi = 2.
13. `stepi`
14. Is it something like `ddd` or just `list`?
15. `backtrace`, which shows the stack of calling functions
16. `b recur if i == 0`
17. `continue`
18. `backtrace`
19. `argc` is 1? Why?
20. two `next`
21. `disas`
22. The reture value is stored in `%eax`
```asm
mov $0x0, %eax
leaveq
retq
```
23-26. continue && quit