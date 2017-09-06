
{
  "targets": [
    {
      "target_name": "autopsy",
      "sources": [ "autopsy.cc" , "autopsy_node.cc" ],
      "cflags": ["-Wall", "-std=c++11"],
      "xcode_settings": {
        "OTHER_CFLAGS": [
          "-std=c++11"
        ]
      }
    }
  ]
}
