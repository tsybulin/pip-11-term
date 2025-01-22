#!/bin/sh

cd piterm 

tftp <<EOF
verbose
connect 172.16.103.103
mode binary
put kernel7l.img
quit
EOF

cd ..
