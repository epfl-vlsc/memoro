
{
  "targets": [
    {
      "target_name": "memoro",
      "sources": [ "memoro.cc" , "memoro_node.cc", "pattern.cc", "stacktree.cc" ],
      "cflags": ["-Wall", "-std=c++14"],
      "xcode_settings": {
        "OTHER_CFLAGS": [
          "-std=c++14"
        ]
      }
    }
  ]
}
