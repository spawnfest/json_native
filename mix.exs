defmodule JasonNative.MixProject do
  use Mix.Project

  def project do
    [
      app: :jason_native,
      version: "0.1.0",
      elixir: "~> 1.14",
      start_permanent: Mix.env() == :prod,
      deps: deps(),
      compilers: [:elixir_make] ++ Mix.compilers(),
      make_clean: ["clean"]
    ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp deps do
    [
      {:elixir_make, "~> 0.4", runtime: false}
    ]
  end
end
