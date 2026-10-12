#!/bin/sh
# Starts the bundled İzekip UI demo. The demo reads style.css and its fonts from its own folder,
# so the script changes into that folder first.
cd /usr/libexec/izekip-ui-demo || exit 1
exec ./IzekipUIExample04 "$@"
