# Calls

A phone dialer and call handler.

## License

Calls is licensed under the GPLv3+.

## Dependencies
To build Calls you need to first install the build-deps defined by [the debian/control file](https://gitlab.gnome.org/GNOME/calls/blob/master/debian/control#L8)

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
with `-Ggtk_doc=true`

    meson . _build -Dgtk_doc=true
    ninja -C _build
    ninja -C _build calls-doc

## Running from the source tree

The most comfortable way to run from the source tree is by using the provided
run script which sets up the environment for you:

    _build/run

## Debugging

When trying to understand issues in applications debugging logs are invaluable
tools. Enable debug logging by invoking Calls with `-vvv` arguments.

In the case of crashes you should provide a backtrace where possible.
If your system is using systemd you may find
[this guide](https://developer.puri.sm/Librem5/Development_Environment/Boards/Troubleshooting/Debugging.html)
useful.

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


### SIP

This plugin uses the libsofia-sip library for SIP signalling and
GStreamer for media handling. It supports multiple SIP accounts and
currently one active call at a time (subject to change).

### Dummy

This plugin is mostly useful for development purposes and work on the UI
as it allows simulating both outgoing and incoming calls. To trigger an
incoming call you should send a `USR1` signal to the calls process:

    kill -SIGUSR1 $(pidof gnome-calls)

### oFono

This plugin is not in active development anymore, so your mileage may vary.
See [here](ofono.md) for more information.
