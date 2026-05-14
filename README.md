# Simple temperature monitor

Outputs the current GPU, CPU, and harddrive temperatures in celcius or farhenheit. On the project roadmap: a way to pass values to scripts to trigger events.


## Compilation Instructions

### GCC
`cc -std=c89 -Wall -Wextra -pedantic -o temp temp.c`

### Bruce's C Compiler (BCC)
`bcc -ansi -f -o temp temp.c`
