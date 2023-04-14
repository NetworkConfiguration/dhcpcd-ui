# dhcpcd-ui

dhcpcd-ui is the graphical interface to
[dhcpcd](http://roy.marples.name/projects/dhcpcd).

It has a helper library in C to try and minimize any toolkit
specific parts.

There are GTK+-2 (works with GTK+-3) and Qt (works with Qt-4 and Qt-5)
front ends.
dhcpcd-curses is very much a work in progress and is only informative
at this stage.

dhcpcd-online can report on network availability from dhcpcd
(requires dhcpcd-6.4.4)

---

## Build options

Switches to control building of various parts:
  *  `--with-dhcpcd-online`
  *  `--with-gtk`
  *  `--with-qt`
  *  `--with-icons`
  *  `--enable-notification`
For each `--with` there is a `--without` and for each `--enable` a `--disable`.
If each part is not specified then the configure will test the system
for the needed libraries to build and install it.

(cariosvg)[https://cairosvg.org/) is used to build the icons from the svg source.
It's not a runtime dependency.

### Notifications

Notifications are dependant on the chosen platform.
GTK+ will get them if libnotify is present.
Qt will get them by default.
