# Jason.Native

This is an extension of the [Jason](https://github.com/michalmuskala/jason) package -
the blazing fast JSON encoder for Elixir.

This package adds a "sprinke of NIFs" to Jason to make it even faster. All you need
to do, is just add `jason_native` as a dependency to your project, and that's it!
No code changes required, things just will be faster.

## Installation

The package can be installed by adding `jason_native`, alongside `jason`
to your list of dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:jason, "~> 1.4"},
    {:jason_native, "~> 0.1.0"}
  ]
end
```

Documentation can be generated with [ExDoc](https://github.com/elixir-lang/ex_doc)
and published on [HexDocs](https://hexdocs.pm). Once published, the docs can
be found at <https://hexdocs.pm/jason_native>.

## Disclaimer

This package uses NIFs - native functions implemented in C++. I made all efforts to
implement things correctly and test them, but native code is tricky.
This **can cause instability** in your systems, please test before deploying in production
to your entire fleet of machines.
