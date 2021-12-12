# TODO
## In AWIT REPL
- Apply NodeJS multiline REPL. Refer to [their](https://nodejs.org/api/repl.html#repl_commands_and_special_keys) documentation.
## Apply Optimizations
- Refer to Crafting Interpreters [Chapter 30](https://craftinginterpreters.com/optimization.html)
## Add support for MySQL database
- Refer to [MySQL C-api documentation](https://dev.mysql.com/doc/c-api/8.0/en/?fbclid=IwAR3NbiENwKs2E34abmu8k_4154q9T0HGSBBPRLLXbm3ButKpAhmecgOcgoc)
> The language does not support multi-threading and asynchrous calls at the moment so the functionality it can currently support is the synchronous blocking queries.
## Handle multiple bytes of array size
- The VM only allows a byte for the array size; that is `2^8 - 1` or `255` in maximum. Add support for multi-bytes encoding for array sizes greater than `255`. Java, for reference has `2^31 - 1` or `2,147,483,647`.
## ~Fix recursion frame stack bug~
- ~Function's call frame stack overflows making the interpreter emit stack overflow error.~
- The problem was the `n / 2` expression where `n` is a number. `/` leaves the floating `.5` value in `n`. This was fixed by using `\` or the integer division in which it truncates the trailing floating values such as the `.5` in `n / 2`.
