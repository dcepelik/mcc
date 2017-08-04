# Minimalist C Compiler (`mcc`)

(Or maybe Modular C Compiler? Or maybe both, I haven't decided yet.)

## Why do I bother?

(An apology for myself.)

* I want to know what goes into writing a compiler from scratch. Reading books about it
  just won't do anymore.
* I hate Bison and Flex and I actually *enjoy* writing fast lexers and parsers by hand.
  Don't judge me, everone's got a perversion.
* I want to learn how to write complex software and compilers have always attracted me.
* I want a framework to test out new code optimization algorithms without fighting with
  existing code.
* Writing a C compiler in C looks like an opportunity to learn about all the dark corners
  of the language.
* I'm interested in code optimization.
* I'd love to write something I can be proud of (at least a bit).

## Design Goals

* An experimental compiler rather than a production one.
* Make the code robust and reusable.
* Write it so that a reasonably smart person can read it and see how it works.
* Keep it short and terse.

### Front-end

* KISS
* C11-only
* Favor performance over flexibility
* Fast as hell
* Decent static analysis
* Warnings are errors by default

### Back-end

* KISS
* x86-64 only
* Favor flexibility over performance
* Highly modular
* Highly configurable
* Make it easy to hack
