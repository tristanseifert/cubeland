# Resources directory
Here are all the resources that get baked into resource catalogs that are in turn embedded into the binary.

- `fonts`: All fonts used by the UI layer (Dear Imgui) for rendering

- `cacert.pem`: Certificate authorities used for validating server connections. These are the [Mozilla CA store](https://curl.haxx.se/docs/caextract.html) certs, as converted by the curl project. This should be kept reasonably up to date.
