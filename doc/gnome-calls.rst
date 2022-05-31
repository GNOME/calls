.. _gnome-calls(1):

===========
gnome-calls
===========

----------------------
Make and receive calls
----------------------

SYNOPSIS
--------

|   **gnome-calls** [OPTIONS...] [SIP/TEL URI]

DESCRIPTION
-----------

``gnome-calls`` is a GTK based dialer for PSTN and SIP.

It uses ModemManager to talk to the cellular modem for PSTN.
Besides phone calls it also handles USSD.
In the case of SIP it uses sofia-sip for the SIP signalling and GStreamer
pipelines to deal with RTP payload of audio data.

It works on desktops but also adjusts to small screen sizes like smart phones
and other mobile devices.

OPTIONS
-------

``-h, --help``

  Show help options.

``--help-all``

  Show all help options.

``--help-gapplication``

  Show GApplication options.

``--help-gtk``

  Show GTK+ options.

``--version``

  Show program version.

``--p, --provider=PLUGIN``

  The plugins to use as call provider. Can be passed multiple times. Defaults to ModemManger and SIP (``-p mm -p sip``).

``-l, --dial=NUMBER``

  Dial a phone number (no need for the ``tel:`` URI scheme prefix).

``-d, --daemon``

  Whether to present the main window on startup.

``-v, --verbose``

  Enable verbose debug messages. Can be passed multiple times.
