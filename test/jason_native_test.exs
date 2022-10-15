defmodule JasonNativeTest do
  use ExUnit.Case
  doctest JasonNative

  test "greets the world" do
    assert JasonNative.hello() == :world
  end
end
