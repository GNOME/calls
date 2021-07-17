# Calls
[![Code coverage](https://gitlab.gnome.org/GNOME/calls/badges/master/coverage.svg)](https://gitlab.gnome.org/GNOME/calls/commits/master)

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
There is also an oFono backend, "ofono".  This was the first backend
developed but has been superceded by the ModemManager backend so it
may suffer from a lack of attention.

The ofono backend depends on oFono Modem objects being present on
D-Bus.  To run oFono with useful output:

    sudo OFONO_AT_DEBUG=1 ofonod -n -d

The test programs within the [oFono source
tree](https://git.kernel.org/pub/scm/network/ofono/ofono.git) are
useful to bring up a modem to a suitable state.  For example:

    cd $OFONO_SOURCE/test
    ./list-modems
    ./enable-modem /sim7100
    ./online-modem /sim7100

Then run Calls:

    /usr/bin/gnome-calls -p ofono


#### Phonesim
One can also make use of the oFono modem simulator, phonesim (in the
ofono-phonesim package in Debian):

    ofono-phonesim -p 12345 -gui /usr/local/share/phonesim/default.xml

then, ensuring /etc/ofono/phonesim.conf has appropriate contents like:

    [phonesim]
    Address=127.0.0.1
    Port=12345

run oFono as above, then:

    cd $OFONO_SOURCE/test
    ./enable-modem /phonesim
    ./online-modem /phonesim

And again run Calls.
