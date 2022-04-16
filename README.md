# C-Coroutine
This is a C coroutine lib.

Test it by
```bash
cd libco && make all
cd tests && make test
```
## Note

Compile with `CFLAGS += -U_FORTIFY_SOURCE` in case the errors raised by `__longjmp_chk` after finding the switch of stack (trait it as stack smashing).

Google's sanitizer met the same problem: https://github.com/google/sanitizers/issues/721
