# Pinned Fune SDK subset

This directory contains unmodified files from Fune, pinned by `UPSTREAM.json`. The SDK entry header is MIT; included source headers, DSP helpers and build/install tools are MPL-2.0. All original license headers and license texts are retained. ZaaggenZ-FM1 uses the host interface and image builder without modifying the firmware.

`tools/build_fune.py` supplies project include paths and a temporary work directory to the original image builder. No upstream code is silently patched.
