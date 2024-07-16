# Calls

A phone dialer and call handler.

## License

Calls is licensed under the GPLv3+.

## Dependencies
To build Calls you need to first install the build-deps defined by [the debian/control file](https://gitlab.gnome.org/GNOME/calls/blob/main/debian/control#L8)

If you are running a Debian based distribution, you can easily install all those the dependencies making use of the following command

    sudo apt-get build-dep .

## Building

We use the meson and thereby Ninja.  The quickest way to get going is
to do the following:

    meson . _build
    ninja -C _build
    ninja -C _build install

If you don't want to pollute your filesystem please be aware, that you can also
use `--prefix=~/install`.

### Build the documentation
If you want to build the documentation you have to configure the meson project
with `-Dgtk_doc=true`

    meson . _build -Dgtk_doc=true
    ninja -C _build
    ninja -C _build calls-doc

You can also browse the documentation [online](https://gnome.pages.gitlab.gnome.org/calls/)

## Running from the source tree

The most comfortable way to run from the source tree is by using the provided
run script which sets up the environment for you:

    _build/run

*Note:* Invoking the run script might terminate after a few seconds and
instead only activates an already running instance of `gnome-calls`.
In this case you want to `systemctl --user stop calls` to stop it.

### Debugging

We want to encourage users to submit high quality bug reports.
We understand that it can be daunting to make sense of a misbehaving application
so following are some tips that can help to pin down the root of a problem.

Enable verbose logging by invoking Calls with `-vvv` argument
(i.e. `_build/run -vvv`).
Observe the output and check for anything suspicious.

If you have found something weird in the logs
you can`grep` the sources to find where the warning is emitted
and - if you're so inclined - start exploring from there:
- check surrounding code
- add debugging output
- set breakpoints and inspect stack traces and local variables

You can also easily run under `gdb` if you invoke like

    CALLS_GDB=1 _build/run -vvv

In the case of crashes you should provide a backtrace ideally with debugging symbols
(for debian based distros you have to add a suitable debugging suite to your apt sources; see link below).

If your system is using systemd you may find
[this guide](https://developer.puri.sm/Librem5/Development_Environment/Boards/Troubleshooting/Debugging.html)
useful: With `coredumpctl` coredumps can easily be analyzed at a later date.

For backend specific debugging, please see the sections below.

## Call provider backends

Calls uses libpeas to support runtime loadable plugins which we call "providers".
Calls currently ships four different plugins:

- mm: The ModemManager plugin used for cellular modems
- sip: The SIP plugin for VoIP
- dummy: A dummy plugin
- ofono: The oFono plugin used for cellular modems (not in active development)

By default Calls will load the `mm` and `sip` plugins.
If you want to load other plugins you may specify the `-p <PLUGIN>` argument
(you can pass multiple `-p` arguments) when invoking calls, f.e.

    _build/run -p sip -p dummy
    /usr/bin/gnome-calls -p mm

Every plugins uses the following concepts:
- CallsProvider: The principal abstraction of a library allowing to place and
receive calls.
- CallsOrigin: Originates calls. Represents a single modem or VoIP account.
- CallsCall: A call.

There is a one to many relation between provider and origins and between origins
and calls. F.e. you have one SIP provider managing multiple SIP accounts (=origins)
each of which can have multiple active calls (not yet implemented).

### ModemManager

This is the default backend for cellular calls. It uses `libmm-glib` to
talk to ModemManager over DBus. It currently only supports one modem and
one active call at a time.

#### Debugging

You can monitor the ModemManager messages on the DBus as follows:

    gdbus monitor --system --dest org.freedesktop.ModemManager1

For complete debugging logs you can set ModemManager's log verbosity to DEBUG as follows:

    mmcli -G DEBUG

and inspect the logs on a systemd based system with:

    journalctl -u ModemManager.service

For more information see [here](https://modemmanager.org/docs/modemmanager/debugging/)

### SIP

This plugin uses the [sofia-sip][] library for SIP signalling and
GStreamer for media handling. It supports multiple SIP accounts and
currently one active call at a time (subject to change).

#### Debugging

You can print the sent and received SIP messages by setting the environment variable
`TPORT_LOG=1`. To test the audio quality you can use one of the various public
reachable echo test services, f.e. echo@conference.sip2sip.info. Please note that
the SIP plugin currently doesn't support DTMF, which is used for some test
services for navigating through a menu.

If one or both sides can't hear any audio at all it is likely that the audio
packets are not reaching the desired destination.

### Dummy

This plugin is mostly useful for development purposes and work on the UI
as it allows simulating both outgoing and incoming calls. To trigger an
incoming call you should send a `USR1` signal to the calls process:

    kill -SIGUSR1 $(pidof gnome-calls)

### oFono

This plugin is not in active development anymore, so your mileage may vary.
See [here](ofono.md) for more information.

[sofia-sip]: https://github.com/BelledonneCommunications/sofia-sip
