# TODO
## In AWIT REPL
- Apply NodeJS multiline REPL. Refer to [their](https://nodejs.org/api/repl.html#repl_commands_and_special_keys) documentation.
## Apply Optimizations
- Refer to Crafting Interpreters [Chapter 30](https://craftinginterpreters.com/optimization.html)
## ~Fix recursion frame stack bug~
- ~Function's call frame stack overflows making the interpreter emit stack overflow error.~
- The problem was the `n / 2` expression where `n` is a number. `/` leaves the floating `.5` value in `n`. This was fixed by using `\` or the integer division in which it truncates the trailing floating values such as the `.5` in `n / 2`.
