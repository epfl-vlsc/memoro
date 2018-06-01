
{
  "targets": [
    {
      "target_name": "memoro",
      "sources": [ "memoro.cc" , "memoro_node.cc", "pattern.cc", "stacktree.cc" ],
      "cflags": ["-Wall", "-std=c++11"],
      "xcode_settings": {
        "OTHER_CFLAGS": [
          "-std=c++11"
        ]
      }
    }
  ]
}
