#!/bin/bash
echo $UUID
modprobe libcomposite
# max and default size for socket messages = 2MB (may need to be increased in case of higher resolution)
sysctl net.core.wmem_max=2097152
sysctl net.core.wmem_default=2097152
# Start Snowmix
(
    export SNOWMIX=/usr/lib/Snowmix-0.5.1.1/
    snowmix base.ini
) &
# Wait for Snowmix to be ready
sleep 2
# Start gstreamer X11 desktop capture to Snowmix feed
(
#!/bin/bash
# File: run_headless_dwm.sh
# Usage: ./run_headless_dwm.sh

# 1️⃣ Start virtual X server (headless)
    export DISPLAY=:1
    Xvfb :1 -screen 0 800x480x24 &

    # Wait a moment for Xvfb to start
    sleep 2

    # 2️⃣ Start minimal window manager (dwm)
    # Run in background; keep Xvfb alive
    dwm &

    # Wait for dwm to initialize
    sleep 2

    # 3️⃣ Start X image stream to /tmp/aacs_feed1
    gst-launch-1.0 ximagesrc use-damage=false ! \
        video/x-raw,framerate=30/1 ! \
        videoconvert ! video/x-raw,format=BGRA ! \
        shmsink socket-path=/tmp/aacs_feed1 &
    XIMAGE_PID=$!

    # Give it a moment to start
    sleep 1

    # 4️⃣ Start video in the X session
    gst-launch-1.0 playbin uri=file:///home/radxa/AACS/build/AAServer/video.mp4 video-sink=ximagesink

    # Optional: clean up on exit
    kill $XIMAGE_PID
) &
(
i=0;
while true; do
    date;
    rm -f socket
    ./AAServer
    sleep 1;
    ((i++));
done
)&
(
while true; do
    sleep 1;
    sync;
done
)