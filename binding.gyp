{
  "targets": [
    {
      "target_name": "PicoHTTPParser",
      "sources": [ "src/pico-http-parser.cc" ],
      "include_dirs": [
        "src",
        "<(module_root_dir)/deps",
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}