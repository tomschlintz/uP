# Notes on the use of the socat and screen applications in Linux
The screen utility is very useful for starting an application, then detaching the terminal so that the terminal may be closed while leaving the application running. This is useful in particular when remoting in to a machine, starting up a server of some kind, then detaching so you can close the ssh connection.

The socat utility is useful for many things, but for testing, we use it to create a virtual loop-back serial port.

## socat
* `socat -d -d pty,raw,echo=0 pty,raw,echo=0 &` will create a software loop-back between two devices in `/dev/pts/`. The output of the command will tell which two.
* `killall socat` will kill the loop-back devices.
* Once loop-back serial devices are created by socat, you can open up two terminal windows and link stdio to each:
  * `screen /dev/pts/3` on one terminal window
  * `screen /dev/pts/4` on the other (assuming socat gave you `/dev/pts/3` and `/dev/pts/4` as the serial devices)
  * Ctrl-a + d detaches the screen, and `killall screen`

## screen
### From [geeksforgeeks.org](https://www.geeksforgeeks.org/screen-command-in-linux-with-examples/)
* Ctrl-a + c: It create a new windows.
* Ctrl-a + w: It display the list of all the windows currently opened.
* Ctrl-a + A: It rename the current windows. The name will appear when you will list the list of windows opened with Ctrl-a + w.
* Ctrl-a + n: It go to the next windows.
* Ctrl-a + p: It go to the previous windows.
* Ctrl-a + Ctrl-a: It back to the last windows used.
* Ctrl-a + k: It close the current windows (kill).
* Ctrl-a + S: It split the current windows horizontally. To switch between the windows, do Ctrl-a + Tab.
* Ctrl-a + |: It split the current windows vertically.
* Ctrl-a + X: Close active Split window
* Ctrl-a + Q: Close all Split windows
* Ctrl-a + d: It detach a screen session without stopping it.
* Ctrl-a + r: It reattach a detached screen session.
* Ctrl-a + [: It start the copy mode.
* Ctrl-a + ]: It paste the copied text.
### Command-line options
* -ls shows a list of screens
* -S <name> creates a named session
### Other useful tricks and tips
* `screen -X -S <name> kill` kills a screen
* `killall screen` kills all screen processes
* `screen -x` works to re-attach when only one screen is created
### To create split screen, one session, but two independent bash windows
1. Create a session
2. Ctrl-a + | or Ctrl-a + S to split the screen
3. Ctrl-a + tab to change to the other split
4. Ctrl-a + c to create a window in that split

_seems to me it's less trouble just to open up two terminal windows!_