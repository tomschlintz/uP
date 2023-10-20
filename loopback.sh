#create the two linked pseudoterminals
socat -d -d pty,raw,echo=0 pty,raw,echo=0 &
ps | grep socat
ls /dev/pts
echo Run "screen /dev/pts/<num>" now to use as terminal
echo Run ctrl-a,d to unlink screen
echo Then run ./unlink_loopback.sh to kill the loopback and screens

