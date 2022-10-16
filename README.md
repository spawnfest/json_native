# Jason.Native

This is an extension of the [Jason](https://github.com/michalmuskala/jason) package -
the blazing fast JSON encoder for Elixir.

This package adds a "sprinke of NIFs" to Jason to make it even faster. All you need
to do, is just add `jason_native` as a dependency to your project, and that's it!
**No code changes are required, things just will be faster.**

This is achieved by making `jason_native` an optional dependency of `jason`, and
switching implementations at compile-time based on the presence (or absence)
of this optional library. This supports all features of `Jason` including
the `Jason.Encode` protocol.

NIFs are implemented respecting all constraints of the BEAM, including periodic
yielding to allow scheduler threads to re-schedule processes.

Currently, `Jason.Native` only adds native routines for encoding strings - one of the
most expensive routnes in encoding JSON.
In microbenchmarks, this speeds up `Jason.encode!` about 1.5 times for most inputs,
and places it **between 30% slower and 40% faster compared to [`jiffy`](https://github.com/davisp/jiffy)**,
which is fully implemented in C.
For some specific inputs - e.g. large amounts of Unicode or extremely long strings,
`Jason.Native` is significantly faster -
**up to 8x faster than regular pure-Elixir `Jason` and 6x faster than pure-C `jiffy`**.

## Project contributions

Beyond the immediate contributions of this project in making `Jason` faster,
with this project, I'd like to promote the "sprinkle of NIFs" technique
for optimising Elixir & Erlang libraries. By providing 2 identical implementations:
Elixir-based and NIF-based, with an easy & transparent way to switch, projects
can achieve both - stability & great performance.

## Installation

The package can be installed by adding `jason_native`, alongside `jason`
to your list of dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:jason, "~> 1.5.0-alpha.1"},
    {:jason_native, "~> 0.1.0"}
  ]
end
```

Other than Elixir 1.14 and compatible Erlang, this package requires:
* `make`
* A C++20-compatible C++ compiler, e.g. `clang` or `gcc`.

Please refer to `Makefile` to supported options for native compilation.

Full documentation can be found at [https://hexdocs.pm/jason_native](https://hexdocs.pm/jason_native).

## Disclaimer

This package uses NIFs - native functions implemented in C++. I made all efforts to
implement things correctly and test them, but native code is tricky.
This **can cause instability** in your systems, please test before deploying in production
to your entire fleet of machines.

## Benchmarks

Benchmarks can be found in the main `Jason` library. Please refer to instructions included
[there](https://github.com/michalmuskala/jason#benchmarks).

Basic results from my machine can be found in [this gist](https://gist.github.com/michalmuskala/814c2fb9dcb3337c6de8ec24a7dad744).

## Future work

Similar technique - "sprinkle of NIFs" should be applicable to the parsing-side of Jason.
In particular, once again, parsing strings. I believe similar, if not significicantly
bigger performance improvements could be achieved.

Furthermore, I believe some "to string" routines inside of the BEAM could be considerably
improved, in particular: `integer_to_binary` and `<<_::utf8>>` binary patterns.

## License

Jason.Native is part of Jason and is released under the Apache License 2.0 - see the [LICENSE](LICENSE) file.

Jason.Native contains unicode-encoding routines with
Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
under the MIT license, see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
