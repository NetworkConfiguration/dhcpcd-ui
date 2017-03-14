# dhcpcd-ui

dhcpcd-ui is the graphical interface to
[dhcpcd](http://roy.marples.name/projects/dhcpcd).

It has a helper library in C to try and minimize any toolkit
specific parts.

There are GTK+-2 (works with GTK+-3) and Qt-4 front ends.
an EFL (e17) front end is planned.

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
