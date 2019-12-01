An example stereo delay LADSPA plugin written in both `Rust` and `C`
to compare their performance.

### Motivation

It's been a while since I have the idea of writing some
[LADSPA](https://ladspa.org/) plugins in a language other `C`. Why?
Because there is a number of more recent languages improving both the
process of writing code and making it more save and resilient per
default by a margin.

Modern `C++` would be a candidate but it's rather complex, grew too
naturally, and it does not provide enough helpful tools, like default
linter, documentation, and test utils, nor a compiler easing the
development.

Since LADSPA plugins are meant to be used in realtime computation, the
language of choice must not be garbage collected. Therefore
[Go](https://golang.org/) is off the table. This pointed to
[Rust](https://www.rust-lang.org/). For a good introduction I can
recommend [the book](https://doc.rust-lang.org/book/).

### Result

Before jumping right into plugin development one has to access whether
the LADSPA plugins written in `Rust` are sufficiently efficient. Of
course, a little bit of performance can be sacrificed for the sake of
using a more convenient and resilient language.

But it turned out - as can be seen in
[analysis.ipynb](analysis.ipynb) - that the `Rust` shared objects are
far less efficient than their `C` counterparts. Despite of using the C
ABI, the ability of Rust code to be called from C without any overhead,
and some performance optimization settings provided to the `rustc`
compiler the `Rust` version does perform a lot worse.

In the end, `Rust` does not seem to be a proper replacement of `C` in
realtime digital signal processing.

... but what about modern `C++`? In the next steps I will translate
the stereo plugin from `C` and compare their performance.

### Usage

First of all, you have to build both LADSPA plugins. For the `C` part
run

``` bash
cd c
make
```

and for the `Rust` part run

``` bash
cd rust
cargo build --release
```

The `--release` argument is quite important since it triggers the
optimization of the compiled code.

Afterwards, you can run the analysis by executing the `Jupyter`
notebook (note that an `R` and not a `Python` kernel is used).

``` bash
jupyter notebook analysis.ipynb
```

### Resources

#### insights into LADSPA 
- https://ladspa.org/

#### Rust interface for LADSPA
- https://nwoeanhinnogaehr.github.io/ladspa.rs/ladspa/

#### Optimization of Rust code
- https://github.com/mbrubeck/fast-rust/blob/master/src/021-cargo-profile.md
- https://doc.rust-lang.org/cargo/reference/manifest.html#the-profile-sections

#### Read audio files into R
- https://cran.r-project.org/web/packages/seewave/vignettes/seewave_IO.pdf
