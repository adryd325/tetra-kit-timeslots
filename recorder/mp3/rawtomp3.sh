#!/bin/bash
#
# 2016-07-21  LT  0.0  first release
#

inotifywait -m ../raw/ -e moved_to|
    while read -r dir action file; do
        BASE=${file%.raw};
        BASE=${BASE/..\/raw\//}

        ffmpeg -nostdin -f s16le -ar 8000 -i "../raw/$file" "$BASE".mp3  >/dev/null &
        echo "The file '$file' appeared in directory '$dir' via '$action'"
        # do something with the file
    done