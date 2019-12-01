Rust version of a simple stereo delay. It is based on the example
plugin provided in the
[ladspa.rs](https://github.com/nwoeanhinnogaehr/ladspa.rs) repo
written by Noah Weninger.

# Building

The plugin can be build using the following command

```bash
cargo build --release
```

Note the `--release` argument. It's quite important since it tell the
`rustc` compiler to apply optimization to the code.
