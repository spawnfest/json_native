defmodule Jason.NativeTest do
  use ExUnit.Case
  doctest Jason.Native

  import Jason.Native

  test "greets the world" do
    assert escape_json("hello world") == ~s(hello world)
    assert escape_json("hello\nworld") == ~s(hello\\nworld)
    assert escape_json("\nhello\nworld\n") == ~s(\\nhello\\nworld\\n)

    assert escape_json("\"") == ~s(\\")
    assert escape_json("\0") == ~s(\\u0000)
    assert escape_json(<<31>>) == ~s(\\u001F)
  end
end
