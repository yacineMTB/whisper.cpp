{
  "targets": [
    {
      "target_name": "hello",
      "sources": [ 
        "src/hello.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<(module_root_dir)/../../."
      ],
      "dependencies": [
        "<!@(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": ["NAPI_CPP_EXCEPTIONS"],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags": [
        "-std=c++11",
        "-std=c11"
      ],
      "conditions": [
        ['OS=="mac"', {
          "xcode_settings": {
            "OTHER_CPLUSPLUSFLAGS" : ["-std=c++11"],
            "OTHER_CFLAGS" : ["-std=c11"]
          }
        }],
        ['OS=="linux"', {
          'cflags': [
            '-std=c++11', 
            '-std=c11'
          ]
        }]
      ],
      "libraries": [
        "-L<(module_root_dir)/../../. -lwhisper"
      ]
    }
  ]
}
