Building
========
For build instructions see the README.md

Pull requests
=============
Before filing a pull request run the tests:

```sh
ninja -C _build test
```

Use descriptive commit messages, see

   https://wiki.gnome.org/Git/CommitMessages

and check

   https://wiki.openstack.org/wiki/GitCommitMessages

for good examples.

Coding Style
============
While much of the original codebase was written using [GNU's Coding Style][1],
please try to follow this document for newly written code.
For existing code you should probably try to not mix different code styles too much.
This coding style is heavily inspired/copied from [phosh's Coding Style][2]
which itself is mostly using [libadwaita's Coding Style][3].

These are the differences:

- We're not picky about GTK+ style function argument indentation, that is
  having multiple arguments on one line is also o.k when there are only two arguments.
- For callbacks we prefer the `on_<action>` pattern e.g.
  `on_feedback_ended ()` since this helps to keep the namespace
  clean.
- Since we're not a library we usually use `G_DEFINE_TYPE` instead of
  `G_DEFINE_TYPE_WITH_PRIVATE` (except when we need a deriveable
  type) since it makes the rest of the code more compact.

Source file layout
------------------
We use one file per GObject. It should be named like the GObject with
the calls prefix, lowercase and '_' replaced by '-'. So a hypothetical
`CallsThing` would go to `src/calls-thing.c`. The
individual C files should be structured as (top to bottom of file):

  - License boilerplate
    ```c
    /*
     * Copyright (C) year copyright holder
     *
     * SPDX-License-Identifier: GPL-3-or-later
     * Author: you <youremail@example.com>
     */
    ```
  - A log domain
    ```C
    #define G_LOG_DOMAIN "CallsThing"
    ```
    Usually just the GObject.
  - `#include`s:
    Calls configuration (if needed) goes first, then Calls includes, then glib/gtk, then generic C headers. These blocks
    are separated by newline and each sorted alphabetically:

    ```
    #define G_LOG_DOMAIN "CallsThing"

    #include "calls-config.h"

    #include "calls-things.h"
    #include "calls-other-things.h"

    #include <gio/gdesktopappinfo.h>
    #include <glib/glib.h>

    #include <math.h>
    ```

    This helps to detect missing headers in includes.
  - docstring
  - property enum
    ```c
    enum {
      PROP_0,
      PROP_FOO,
      PROP_BAR,,
      LAST_PROP
    };
    static GParamSpec *props[LAST_PROP];
    ```
  - signal enum
    ```c
    enum {
      FOO_HAPPENED,
      BAR_TRIGGERED,
      N_SIGNALS
    };
    static guint signals[N_SIGNALS];
    ```
  - type definitions
    ```c
    typedef struct _CallsThing {
      GObject parent;

      ...
    } CallsThing;

    G_DEFINE_TYPE (CallsThing, calls_thing, G_TYPE_OBJECT)
    ```
  - private methods and callbacks (these can also go at convenient
    places above `calls_thing_constructed ()`
  - `calls_thing_set_properties ()`
  - `calls_thing_get_properties ()`
  - `calls_thing_constructed ()`
  - `calls_thing_dispose ()`
  - `calls_thing_finalize ()`
  - `calls_thing_class_init ()`
  - `calls_thing_init ()`
  - `calls_thing_new ()`
  - Public methods, all starting with the object name(i.e. `calls_thing_`)

  The reason public methods go at the bottom is that they have declarations in
  the header file and can thus be referenced from anywhere else in the source
  file.

  Try to avoid forward declarations where possible.
[1]: https://www.gnu.org/prep/standards/standards.html#Formatting
[2]: https://gitlab.gnome.org/World/Phosh/phosh/-/blob/main/HACKING.md#coding-style
[3]: https://gitlab.gnome.org/GNOME/libadwaita/-/blob/main/HACKING.md#coding-style
