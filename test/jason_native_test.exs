defmodule Jason.NativeTest do
  use ExUnit.Case, async: true
  use ExUnitProperties

  doctest Jason.Native

  import Jason.Native

  test "greets the world" do
    assert escape_json("hello world") == ~S(hello world)
    assert escape_json("hello\nworld") == ~S(hello\nworld)
    assert escape_json("\nhello\nworld\n") == ~S(\nhello\nworld\n)

    assert escape_json("\"") == ~S(\")
    assert escape_json("\0") == ~S(\u0000)
    assert escape_json(<<31>>) == ~S(\u001F)

    assert escape_json("\0\0\0\0\0\0") == ~S(\u0000\u0000\u0000\u0000\u0000\u0000)

    long_text = String.duplicate("a", 64 * 2)

    assert escape_json(long_text <> "\0\0\0\0\0\0") == [
             long_text <> ~S(\u0000\u0000),
             ~S(\u0000\u0000\u0000\u0000)
           ]
  end

  property "escape_json/1" do
    check all(string <- string(:printable)) do
      escaped = IO.iodata_to_binary(escape_json(string))

      for <<(<<byte>> <- escaped)>> do
        assert byte < 0xF1 or byte not in [?", ?\\]
      end

      assert Jason.decode!([?", escaped, ?"]) == string
    end
  end
end
