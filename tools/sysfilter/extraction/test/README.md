# Sysfilter test framework

This directory contains unit tests for the extraction tool.  Tests are
composed of a set of simple C programs and a Python file to run each
test.

Each directory contains a set of related tests.

## Requirements
 - Everything required to build the extraction tool
 - `Python3`

## Running tests

To run all tests, run:
```
make test
```

To run a specific group of tests, simply change to that test directory
and run `make test`.

When each test is run, a `.log` and `.json` file are created with the
output and log file produced by sysfilter, respectively.  To clean up
these files, run `make testclean`.


## How to make new tests

In general, see the test directories `hello` and `raw` for examples.

To create a new test set:
1. Make a new test directory with an example C program
2. Create a makefile to build your program, with targets `all` to
   build all binaries, `clean` to remove object files, and `test` to
   run the tests.
3. Create a blank file named `__init__.py` in the directory.  This
   allows the test discovery framework to find the tests.
3. Create a set of test cases using the Python unit test framework.
4. Add your test directory to `TEST_DIRS` in the makefile located in
   this directory

### Guidelines for writing test cases

When writing test cases, observe the following:
 - Do NOT `assertEqual` the entire syscall set.  This will make the
   tests inflexible to differences in system environments (glibc
   versions, etc)


## FAQ

### I want to run a specific test, what should I do?

From a test directory run:
```
python3 -m unitttest <test_file>.<test_class>.<name_of_test_function>
```

For example:

```
python3 -m unittest test_raw.TestRaw.test_failures
```

### Can I rebuild sysfilter while I'm inside a test directory?

Yes!  Run `make sysfilter`.
