{
  "targets": [
    {
      "target_name": "addon",
      "sources": ["src/unecm.cc" ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}