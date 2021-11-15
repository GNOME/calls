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

## Running
Calls has a variety of backends.  The default backend is "mm", which
utilises ModemManager.  To choose a different backend, use the -p
command-line option.  For example, to run with the dummy backend and
some useful debugging output:

    export G_MESSAGES_DEBUG=all
    /usr/local/bin/gnome-calls -p dummy

If using ModemManager, Calls will wait for ModemManager to appear on
D-Bus and then wait for usable modems to appear.  The UI will be
inactive and display a status message until a usable modem appears.

### Running from the build directory
You can run calls without having to install it by executing the run script in
the build folder, i.e. `_build/run`. This script will setup the needed environment
and start Calls.

### Call provider backends
Call provider backends are compiled as plugins and can be loaded and unloaded at runtime
using the `-p` command line flag, followed by the plugin name.

Setting the `CALLS_PLUGIN_DIR` environment variable will include the specified
directory in the plugin search path. F.e.

    export CALLS_PLUGIN_DIR=_build/plugins/
    /usr/local/bin/gnome-calls -p dummy


### oFono

This plugin is not in active development anymore, so your mileage may vary.
See [here](ofono.md) for more information.
