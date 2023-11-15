1. Execute `gcc -m32 -S -o recurse.S recurse.c` to get `recurse.S`.
The corresponding call of `recur(i-1)` is the followind lines:
```asm
	movl	8(%ebp), %eax
	subl	$1, %eax
	subl	$12, %esp
	pushl	%eax
	call	recur
```
2. `.text` are codes to execute.
`.data` are the global or static data.

3. `objdump --syms map.o`

4. `g`: global symbals
`O`: the name of a object
`F`: the name of a function
`*UND*`: referenced but undefined

5. Execute `objdump --syms recurse.o`
```
00000000 l    df *ABS*  00000000 recurse.c
```

6. It's much longer and add some "global" stuff. `recur` is not undefined as well.

7. `.text`

8. `foo` is in `.bss`, and `stuff` is in `.data`

9. Nothing about stack or heap, because they depend on the runtime and are handled by program dynamicly. The information above are all static.

10. An output example is:
```
i is 3. Address of i is 0xffad3d70
i is 2. Address of i is 0xffad3d50
i is 1. Address of i is 0xffad3d30
i is 0. Address of i is 0xffad3d10
```
So the stack is growing down.