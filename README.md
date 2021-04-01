# Calls

A phone dialer and call handler.

## License

Calls is licensed under the GPLv3+.

## Dependencies
To build Calls you need to first install the build-deps defined by [the debian/control file](https://source.puri.sm/Librem5/calls/blob/master/debian/control#L6)

If you are running a Debian based distribution, you can easily install all those the dependencies making use of the following command

    sudo apt-get build-dep .

## Building

We use the meson and thereby Ninja.  The quickest way to get going is
to do the following:

    meson -Dprefix=/usr/local/stow/calls-git ../calls-build
    ninja -C ../calls-build
    ninja -C ../calls-build install


## Running
Calls has a variety of backends.  The default backend is "mm", which
utilises ModemManager.  To choose a different backend, use the -p
command-line option.  For example, to run with the dummy backend and
some useful debugging output:

    export G_MESSAGES_DEBUG=all
    /usr/local/stow/calls-git/bin/calls -p dummy

If using ModemManager, Calls will wait for ModemManager to appear on
D-Bus and then wait for usable modems to appear.  The UI will be
inactive and display a status message until a usable modem appears.

When running from the source tree you can use `CALLS_PLUGIN_DIR` environment
varible to specify the directroy from where plugins are loaded. To e.g. load
the dummy plugin from the source tree:

    export CALLS_PLUGIN_DIR=_build/plugins/dummy/
    _build/src/gnome-calls -p dummy

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

    /usr/local/stow/calls-git/bin/calls -p ofono


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
