# televideo_screensaver

## Screen saver with password lock for old school televideo terminals (tested on Televideo 910)

Forked from https://github.com/mezantrop/sclocka

should run on *BSD, macOS and Linux (tested on macOS only)

### Differences from Sclocka

- no animation whatsoever - screen will just be blanked
- removed window resize support (not needed on real hardware terminals)
- screen size harcoded to 80x24 (as supported on Televideo terminals)
- supports Televideo escape sequences
- correctly handles cursor show and hide function for these terminals
- invokes a login shell when launched
- will un-blank and redraw screen when new data arrives (when -B b, which is default)
- when redrawing screen, it will redraw it correctly, without lines already out of view, no scrolling
- redraw of the screen is much faster (well, hw baud rate will make it seem slow still, LOL)
- hitting space bar when terminal is locked will redraw screen for the set time then back to blank
- hitting any other key when terminal is locked will ask for password

### Build and install

Build Requirements

- CLANG or GCC
- make
- PAM development libraries

#### FreeBSD

`# make install clean`

Note, the default `login` PAM service on FreeBSD doesn't check a user's password for the same login. To allow `tvi_screensaver`
perform password authentication via PAM, install [unix-selfauth-helper](https://github.com/Zirias/unix-selfauth-helper):

```sh
# pkg install unix-selfauth-helper
```

Create and configure a special PAM service e.g. /etc/pam.d/tvi_screensaver:

```
auth            sufficient      pam_exec.so             return_prog_exit_status expose_authtok /usr/local/libexec/unix-selfauth-helper
auth            include         system
account         include         system
```

Start `tvi_screensaver` with `-P tvi_screensaver` option:

```sh
$ tvi_screensaver -P tvi_screensaver
```

#### NetBSD

`# make install clean`

See notes about PAM helper from FreeBSD section

#### OpenBSD

No PAM supported

`# make without_pam install clean`

#### macOS

`# make install clean`

Note, you may be prompted to install command line developer tools

#### Debian flavor Linux

Make sure you have everything to compile sources:

```sh
$ sudo apt-get install build-essential
$ sudo apt-get install libpam0g-dev

$ sudo make install clean
```

#### Red Hat based Linux

Make sure you have everything to compile sources:

```sh
$ sudo dnf groupinstall "Development Tools"
$ sudo dnf install pam-devel

$ sudo make install clean
```

### Run

`# tvi_screensaver`

Wait for a screensaver to appear

#### Options

`tvi_screensaver` can be invoked with several options:

```sh
$ tvi_screensaver -h

Usage:
        tvi_screensaver [-b n|f|b|c] [-c] [-i n] [-p] [-P service] [-h]

[-b f]          Screen restore: (n)one, (f)ormfeed, (b)uffer, (c)apabilities
[-c]            Do not clear the window
[-i X]          Wait X minutes before blanking the screen
[-p]            Disable PAM password check (terminal is not locked just blanked)
[-P login]      Use custom PAM service

[-h]            This message
